#include "processor/operator/intersect/intersect.h"

#include <algorithm>
#include <cstring>

#include "function/hash/hash_functions.h"
#include "processor/operator/intersect/intersect_kernels.h"
#include "processor/result/factorized_table.h"

using namespace riag::common;

namespace riag {
namespace processor {

std::string IntersectPrintInfo::toString() const {
    std::string result = "Key: ";
    result += key->toString();
    return result;
}

void Intersect::initLocalStateInternal(ResultSet* resultSet, ExecutionContext* /*context*/) {
    // Clear member vectors so re-execution on a reused operator starts clean.
    probeKeyVectors.clear();
    payloadColumnIdxesToScanFrom.clear();
    payloadVectorsToScanInto.clear();
    intersectSelVectors.clear();
    isIntersectListAFlatValue.clear();
    outKeyVector = resultSet->getValueVector(outputDataPos);
    for (auto& dataInfo : intersectDataInfos) {
        probeKeyVectors.push_back(resultSet->getValueVector(dataInfo.keyDataPos));
        std::vector<uint32_t> columnIdxesToScanFrom;
        std::vector<ValueVector*> vectorsToReadInto;
        for (auto i = 0u; i < dataInfo.payloadsDataPos.size(); i++) {
            auto vector = resultSet->getValueVector(dataInfo.payloadsDataPos[i]);
            // Always skip the first two columns in the fTable: build key and intersect key.
            // TODO(Guodong): Remove this assumption so that keys can be stored in any order. Change
            // mapping logic too so that we don't need to maintain this order explicitly.
            columnIdxesToScanFrom.push_back(i + 2);
            vectorsToReadInto.push_back(vector.get());
        }
        payloadColumnIdxesToScanFrom.push_back(columnIdxesToScanFrom);
        payloadVectorsToScanInto.push_back(std::move(vectorsToReadInto));
    }
    for (auto& sharedHT : sharedHTs) {
        intersectSelVectors.push_back(std::make_unique<SelectionVector>(DEFAULT_VECTOR_CAPACITY));
        isIntersectListAFlatValue.push_back(
            sharedHT->getHashTable()->getTableSchema()->getColumn(1)->isFlat());
    }
    alignedRightList.resize(DEFAULT_VECTOR_CAPACITY);
    flatTuplesToIntersect.assign(sharedHTs.size(), nullptr);
    listsToIntersect.assign(sharedHTs.size(), overflow_value_t{});
    listIdxes.assign(sharedHTs.size(), 0);
    selVectorsForIntersectedLists.clear();
    selVectorsForIntersectedLists.reserve(sharedHTs.size());
    for (auto& tuples : probedFlatTuples) {
        tuples.clear();
    }
    std::fill(tupleIdxPerBuildSide.begin(), tupleIdxPerBuildSide.end(), 0);
    carryBuildSideIdx = -1u;
    duplicateRunKeys.clear();
    duplicateRunBounds.clear();
    duplicateRunCursor.assign(sharedHTs.size(), 0);
    nextDuplicateRun = 0;
}

void Intersect::probeHTs() {
    hash_t hashVal = 0;
    for (auto i = 0u; i < probeKeyVectors.size(); i++) {
        DASSERT(probeKeyVectors[i]->state->isFlat());
        probedFlatTuples[i].clear();
        if (sharedHTs[i]->getHashTable()->getNumEntries() == 0) {
            continue;
        }
        auto key =
            probeKeyVectors[i]->getValue<nodeID_t>(probeKeyVectors[i]->state->getSelVector()[0]);
        function::Hash::operation<nodeID_t>(key, false, hashVal);
        auto flatTuple = sharedHTs[i]->getHashTable()->getTupleForHash(hashVal);
        while (flatTuple) {
            nodeID_t tupleKey;
            memcpy(&tupleKey, flatTuple, sizeof(nodeID_t));
            if (tupleKey == key) {
                probedFlatTuples[i].push_back(flatTuple);
            }
            flatTuple = sharedHTs[i]->getHashTable()->getPrevTupleValue(flatTuple);
        }
    }
}

void Intersect::twoWayIntersect(nodeID_t* leftNodeIDs, SelectionVector& lSelVector,
    nodeID_t* rightNodeIDs, SelectionVector& rSelVector) {
    DASSERT(lSelVector.getSelSize() <= rSelVector.getSelSize());
    auto leftPositionBuffer = lSelVector.getMutableBuffer();
    auto rightPositionBuffer = rSelVector.getMutableBuffer();
    const auto outputValuePosition =
        intersectNodeIDs(leftNodeIDs, lSelVector.getSelSize(), rightNodeIDs,
            rSelVector.getSelSize(), leftPositionBuffer.data(), rightPositionBuffer.data());
    lSelVector.setToFiltered(outputValuePosition);
    rSelVector.setToFiltered(outputValuePosition);
}

// Overwrites every entry of listsToIntersect, which has one entry per tuple.
static void fetchListsToIntersectFromTuples(const std::vector<uint8_t*>& tuples,
    const std::vector<bool>& isFlatValue, std::vector<overflow_value_t>& listsToIntersect) {
    DASSERT(listsToIntersect.size() == tuples.size());
    for (auto i = 0u; i < tuples.size(); i++) {
        if (isFlatValue[i]) {
            listsToIntersect[i] =
                overflow_value_t{1 /* numElements */, tuples[i] + sizeof(nodeID_t)};
        } else {
            // Tuples are packed without alignment padding; use memcpy.
            memcpy(&listsToIntersect[i], tuples[i] + sizeof(nodeID_t), sizeof(overflow_value_t));
        }
    }
}

// Overwrites every entry of listIdxes, which has one entry per list.
static void swapSmallestListToFront(std::vector<overflow_value_t>& lists,
    std::vector<uint32_t>& listIdxes) {
    DASSERT(lists.size() >= 2 && listIdxes.size() == lists.size());
    iota(listIdxes.begin(), listIdxes.end(), 0);
    uint32_t smallestListIdx = 0;
    for (auto i = 1u; i < lists.size(); i++) {
        if (lists[i].numElements < lists[smallestListIdx].numElements) {
            smallestListIdx = i;
        }
    }
    if (smallestListIdx != 0) {
        std::swap(lists[smallestListIdx], lists[0]);
        std::swap(listIdxes[smallestListIdx], listIdxes[0]);
    }
}

static void sliceSelVectors(const std::vector<SelectionVector*>& selVectorsToSlice,
    SelectionVector& slicer) {
    for (auto selVec : selVectorsToSlice) {
        for (auto i = 0u; i < slicer.getSelSize(); i++) {
            auto pos = slicer[i];
            auto buffer = selVec->getMutableBuffer();
            buffer[i] = selVec->operator[](pos);
        }
        selVec->setToFiltered(slicer.getSelSize());
    }
}

void Intersect::intersectLists() {
    if (listsToIntersect[0].numElements == 0) {
        outKeyVector->state->getSelVectorUnsafe().setSelSize(0);
        return;
    }
    DASSERT(listsToIntersect[0].numElements <= DEFAULT_VECTOR_CAPACITY);
    memcpy(outKeyVector->getData(), listsToIntersect[0].value,
        listsToIntersect[0].numElements * sizeof(nodeID_t));
    leftSelVector.setToUnfiltered(listsToIntersect[0].numElements);
    selVectorsForIntersectedLists.clear();
    intersectSelVectors[0]->setToUnfiltered(listsToIntersect[0].numElements);
    selVectorsForIntersectedLists.push_back(intersectSelVectors[0].get());
    // Lists live in packed factorized-table/unflat storage without alignment padding, so the raw
    // value pointer may be misaligned for nodeID_t; copy each right-hand list into aligned storage
    // before intersecting. Each list is one build-side tuple appended from a single ValueVector, so
    // it holds at most DEFAULT_VECTOR_CAPACITY nodeIDs, like the left list in outKeyVector.
    for (auto i = 0u; i < listsToIntersect.size() - 1; i++) {
        const auto& rightList = listsToIntersect[i + 1];
        DASSERT(rightList.numElements <= DEFAULT_VECTOR_CAPACITY);
        if (rightList.numElements > alignedRightList.size()) {
            alignedRightList.resize(rightList.numElements);
        }
        intersectSelVectors[i + 1]->setToUnfiltered(rightList.numElements);
        if (rightList.numElements > 0) {
            memcpy(alignedRightList.data(), rightList.value,
                rightList.numElements * sizeof(nodeID_t));
        }
        twoWayIntersect((nodeID_t*)outKeyVector->getData(), leftSelVector, alignedRightList.data(),
            *intersectSelVectors[i + 1]);
        // Here we need to slice all selVectors that have been previously intersected, as all these
        // lists need to be selected synchronously to read payloads correctly.
        sliceSelVectors(selVectorsForIntersectedLists, leftSelVector);
        leftSelVector.setToUnfiltered();
        selVectorsForIntersectedLists.push_back(intersectSelVectors[i + 1].get());
    }
    outKeyVector->state->getSelVectorUnsafe().setSelSize(leftSelVector.getSelSize());
}

// Lists live in packed storage (see intersectLists), so entries are read with memcpy.
static nodeID_t getListEntry(const overflow_value_t& list, sel_t pos) {
    nodeID_t nodeID;
    memcpy(&nodeID, list.value + pos * sizeof(nodeID_t), sizeof(nodeID_t));
    return nodeID;
}

static bool hasEqualNeighbour(const overflow_value_t& list, sel_t pos, const nodeID_t& key) {
    return (pos > 0 && getListEntry(list, pos - 1) == key) ||
           (pos + 1 < list.numElements && getListEntry(list, pos + 1) == key);
}

// A list holds one entry per relationship, so parallel edges put the same neighbour into it
// several times, and each combination of one entry per list is a separate match. The kernels
// pair equal entries one to one instead (a key in lists of 2 and 2 entries yields 2 matches, not
// 4), which is right only while no matched key repeats in a list. Lists are sorted, so a repeat
// sits next to the matched position; checking both neighbours of every match is the whole cost
// when there are none. Otherwise, record every matched key with the run of positions it covers
// in each list, for emitDuplicateRuns to expand into the cross product.
bool Intersect::collectDuplicateRuns() {
    const auto numLists = listsToIntersect.size();
    const auto numMatches = outKeyVector->state->getSelVector().getSelSize();
    const auto* keys = reinterpret_cast<const nodeID_t*>(outKeyVector->getData());
    auto hasDuplicates = false;
    for (auto i = 0u; i < numMatches && !hasDuplicates; i++) {
        for (auto j = 0u; j < numLists; j++) {
            if (hasEqualNeighbour(listsToIntersect[j], (*intersectSelVectors[j])[i], keys[i])) {
                hasDuplicates = true;
                break;
            }
        }
    }
    if (!hasDuplicates) {
        return false;
    }
    duplicateRunKeys.clear();
    duplicateRunBounds.clear();
    nextDuplicateRun = 0;
    for (auto i = 0u; i < numMatches; i++) {
        const auto key = keys[i];
        // Matches come out in list order, so the pairs the kernels made for one key are adjacent;
        // the first of them is enough to find that key's runs.
        if (!duplicateRunKeys.empty() && duplicateRunKeys.back() == key) {
            continue;
        }
        duplicateRunKeys.push_back(key);
        for (auto j = 0u; j < numLists; j++) {
            const auto& list = listsToIntersect[j];
            const auto pos = (*intersectSelVectors[j])[i];
            auto begin = pos;
            while (begin > 0 && getListEntry(list, begin - 1) == key) {
                begin--;
            }
            auto end = pos + 1;
            while (end < list.numElements && getListEntry(list, end) == key) {
                end++;
            }
            duplicateRunBounds.push_back(begin);
            duplicateRunBounds.push_back(end);
        }
    }
    for (auto j = 0u; j < numLists; j++) {
        duplicateRunCursor[j] = duplicateRunBounds[2 * j];
    }
    return true;
}

// Writes the next at most DEFAULT_VECTOR_CAPACITY rows of the cross product recorded by
// collectDuplicateRuns into outKeyVector and intersectSelVectors, which populatePayloads reads.
// The output can be larger than any input list, so it may take several calls.
void Intersect::emitDuplicateRuns() {
    const auto numLists = listsToIntersect.size();
    auto* keys = reinterpret_cast<nodeID_t*>(outKeyVector->getData());
    sel_t numRows = 0;
    while (hasPendingDuplicateRuns() && numRows < DEFAULT_VECTOR_CAPACITY) {
        keys[numRows] = duplicateRunKeys[nextDuplicateRun];
        for (auto j = 0u; j < numLists; j++) {
            intersectSelVectors[j]->getMutableBuffer()[numRows] = duplicateRunCursor[j];
        }
        numRows++;
        // Advance like an odometer, last list fastest; once the first list wraps, every
        // combination of this key is out and the cursor moves to the next key's runs.
        const auto* bounds = duplicateRunBounds.data() + 2 * numLists * nextDuplicateRun;
        for (auto j = numLists; j-- > 0;) {
            if (++duplicateRunCursor[j] < bounds[2 * j + 1]) {
                break;
            }
            duplicateRunCursor[j] = bounds[2 * j];
            if (j == 0) {
                nextDuplicateRun++;
                if (hasPendingDuplicateRuns()) {
                    const auto* nextBounds = bounds + 2 * numLists;
                    for (auto l = 0u; l < numLists; l++) {
                        duplicateRunCursor[l] = nextBounds[2 * l];
                    }
                }
            }
        }
    }
    for (auto j = 0u; j < numLists; j++) {
        intersectSelVectors[j]->setToFiltered(numRows);
    }
    outKeyVector->state->getSelVectorUnsafe().setSelSize(numRows);
}

void Intersect::populatePayloads() {
    for (auto i = 0u; i < listIdxes.size(); i++) {
        auto listIdx = listIdxes[i];
        sharedHTs[listIdx]->getHashTable()->getFactorizedTable()->lookup(
            payloadVectorsToScanInto[listIdx], intersectSelVectors[i].get(),
            payloadColumnIdxesToScanFrom[listIdx], flatTuplesToIntersect[listIdx]);
    }
}

bool Intersect::hasNextTuplesToIntersect() {
    tupleIdxPerBuildSide[carryBuildSideIdx]++;
    if (tupleIdxPerBuildSide[carryBuildSideIdx] == probedFlatTuples[carryBuildSideIdx].size()) {
        if (carryBuildSideIdx == 0) {
            return false;
        }
        tupleIdxPerBuildSide[carryBuildSideIdx] = 0;
        carryBuildSideIdx--;
        if (!hasNextTuplesToIntersect()) {
            return false;
        }
        carryBuildSideIdx++;
    }
    return true;
}

bool Intersect::getNextTuplesInternal(ExecutionContext* context) {
    do {
        if (hasPendingDuplicateRuns()) {
            // The rest of the previous tuples' cross product. Their lists, flat tuples and probe
            // row are still current: nothing below runs until the runs are drained.
            emitDuplicateRuns();
            populatePayloads();
            continue;
        }
        while (carryBuildSideIdx == -1u) {
            if (!children[0]->getNextTuple(context)) {
                return false;
            }
            // For each build side, probe its HT and return a vector of matched flat tuples.
            probeHTs();
            auto maxNumTuplesToIntersect = 1u;
            for (auto i = 0u; i < getNumBuilds(); i++) {
                maxNumTuplesToIntersect *= probedFlatTuples[i].size();
            }
            if (maxNumTuplesToIntersect == 0) {
                // Skip if any build side has no matches.
                continue;
            }
            carryBuildSideIdx = getNumBuilds() - 1;
            std::fill(tupleIdxPerBuildSide.begin(), tupleIdxPerBuildSide.end(), 0);
        }
        // Cartesian product of all flat tuples probed from all build sides.
        // Notice: when there are large adjacency lists in the build side, which means the list is
        // too large to fit in a single ValueVector, we end up chunking the list as multiple tuples
        // in FTable. Thus, when performing the intersection, we need to perform cartesian product
        // between all flat tuples probed from all build sides.
        for (auto i = 0u; i < getNumBuilds(); i++) {
            flatTuplesToIntersect[i] = probedFlatTuples[i][tupleIdxPerBuildSide[i]];
        }
        fetchListsToIntersectFromTuples(flatTuplesToIntersect, isIntersectListAFlatValue,
            listsToIntersect);
        swapSmallestListToFront(listsToIntersect, listIdxes);
        intersectLists();
        if (outKeyVector->state->getSelVector().getSelSize() != 0) {
            if (collectDuplicateRuns()) {
                emitDuplicateRuns();
            }
            populatePayloads();
        }
        if (!hasNextTuplesToIntersect()) {
            carryBuildSideIdx = -1u;
        }
    } while (outKeyVector->state->getSelVector().getSelSize() == 0);
    metrics->numOutputTuple.increase(outKeyVector->state->getSelVector().getSelSize());
    return true;
}

} // namespace processor
} // namespace riag

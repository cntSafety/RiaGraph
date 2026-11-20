#include "processor/operator/scan/scan_multi_rel_tables.h"

#include "common/assert.h"
#include "processor/execution_context.h"
#include "storage/local_storage/local_storage.h"
#include "storage/table/arrow_rel_table.h"
#include "storage/table/ice_disk_rel_table.h"

using namespace riag::common;
using namespace riag::storage;
using namespace riag::transaction;

namespace riag {
namespace processor {

bool DirectionInfo::needFlip(RelDataDirection relDataDirection) const {
    if (extendFromSource && relDataDirection == RelDataDirection::BWD) {
        return true;
    }
    if (!extendFromSource && relDataDirection == RelDataDirection::FWD) {
        return true;
    }
    return false;
}

bool RelTableCollectionScanner::scan(main::ClientContext* context, RelTableScanState& scanState,
    const std::vector<ValueVector*>& outVectors) {
    auto transaction = Transaction::Get(*context);
    auto initNextTable = [&]() -> bool {
        currentTableIdx = nextTableIdx;
        if (currentTableIdx == relInfos.size()) {
            return false;
        }
        auto& currentInfo = relInfos[currentTableIdx];
        currentInfo.initScanState(scanState, outVectors, context);
        currentInfo.table->initScanState(transaction, scanState, currentTableIdx == 0);
        nextTableIdx++;
        return true;
    };
    if (currentTableIdx == INVALID_IDX && !initNextTable()) {
        return false;
    }
    while (true) {
        auto& relInfo = relInfos[currentTableIdx];
        if (relInfo.table->scan(transaction, scanState)) {
            auto& selVector = scanState.outState->getSelVector();
            if (directionVector != nullptr) {
                for (auto i = 0u; i < selVector.getSelSize(); ++i) {
                    directionVector->setValue<bool>(selVector[i], directionValues[currentTableIdx]);
                }
            }
            if (selVector.getSelSize() > 0) {
                relInfo.castColumns();
                return true;
            }
        } else {
            if (!initNextTable()) {
                return false;
            }
        }
    }
}

void ScanMultiRelTable::refreshNbrMaskCache() {
    refreshNbrMasks(nbrNodeMaskMap.get());
}

common::sel_t ScanMultiRelTable::applyNbrNodeMask() {
    return applyNbrMaskFilter(scanState->outState->getSelVectorUnsafe(), outVectors[0]);
}

void ScanMultiRelTable::initLocalStateInternal(ResultSet* resultSet, ExecutionContext* context) {
    ScanTable::initLocalStateInternal(resultSet, context);
    refreshNbrMaskCache();
    auto clientContext = context->clientContext;
    boundNodeIDVector = resultSet->getValueVector(opInfo.nodeIDPos).get();
    auto nbrNodeIDVector = outVectors[0];

    // Check if any table in any scanner is an external rel table with a custom scan state.
    bool hasArrowTable = false;
    bool hasIceDiskTable = false;
    for (auto& [_, scanner] : scanners) {
        for (auto& relInfo : scanner.relInfos) {
            if (dynamic_cast<storage::ArrowRelTable*>(relInfo.table) != nullptr) {
                hasArrowTable = true;
                break;
            }
            if (dynamic_cast<storage::IceDiskRelTable*>(relInfo.table) != nullptr) {
                hasIceDiskTable = true;
                break;
            }
        }
        if (hasArrowTable || hasIceDiskTable) {
            break;
        }
    }

    // IceDisk scan state extends the common rel scan state and Arrow stores its per-table state
    // there, so one scan state can now cover IceDisk, Arrow, and native rel tables.
    if (hasIceDiskTable) {
        scanState =
            std::make_unique<storage::IceDiskRelTableScanState>(*MemoryManager::Get(*clientContext),
                boundNodeIDVector, outVectors, nbrNodeIDVector->state);
    } else if (hasArrowTable) {
        scanState =
            std::make_unique<storage::ArrowRelTableScanState>(*MemoryManager::Get(*clientContext),
                boundNodeIDVector, outVectors, nbrNodeIDVector->state);
    } else {
        scanState = std::make_unique<RelTableScanState>(*MemoryManager::Get(*clientContext),
            boundNodeIDVector, outVectors, nbrNodeIDVector->state);
    }
    for (auto& [_, scanner] : scanners) {
        for (auto& relInfo : scanner.relInfos) {
            if (directionInfo.directionPos.isValid()) {
                scanner.directionVector =
                    resultSet->getValueVector(directionInfo.directionPos).get();
                scanner.directionValues.push_back(directionInfo.needFlip(relInfo.direction));
            }
        }
    }
    currentScanner = nullptr;
    boundNodeState.reset();
}

bool ScanMultiRelTable::getNextTuplesInternal(ExecutionContext* context) {
    while (true) {
        if (currentScanner != nullptr &&
            currentScanner->scan(context->clientContext, *scanState, outVectors)) {
            const auto filteredSize = applyNbrNodeMask();
            if (filteredSize == 0) {
                continue;
            }
            metrics->numOutputTuple.increase(filteredSize);
            return true;
        }
        boundNodeState.restore(*boundNodeIDVector->state);
        if (!children[0]->getNextTuple(context)) {
            resetState();
            scanState->releaseScanPin();
            return false;
        }
        boundNodeState.save(*boundNodeIDVector->state);
        // The scanner is chosen from the node table of the first selected bound node, and the rel
        // table scan then takes the whole selection of the bound chunk as one batch
        // (RelTable::initScanState reads every selected position as a node of that table). That
        // is only right if the chunk is single-table and has no NULL: a NULL is skipped below by
        // looking at position 0 alone, and a position from another node table would be scanned
        // against the wrong rel tables. Node table scans and rel scans emit one table per batch,
        // and no plan tried (OPTIONAL MATCH, DISTINCT, ORDER BY/LIMIT, UNWIND over multi-label
        // and undirected patterns; the match/optional_match/subquery/order_by/... e2e suites)
        // delivers anything else, so this is checked rather than handled. A producer that merges
        // node tables or NULLs into one unflat chunk has to flatten it, or this operator has to
        // scan per position. (#151)
        RUNTIME_CHECK({
            const auto& boundSelVector = boundNodeIDVector->state->getSelVector();
            if (boundSelVector.getSelSize() > 1) {
                DASSERT(!boundNodeIDVector->isNull(boundSelVector[0]));
                const auto firstTableID =
                    boundNodeIDVector->getValue<nodeID_t>(boundSelVector[0]).tableID;
                for (auto i = 1u; i < boundSelVector.getSelSize(); ++i) {
                    DASSERT(!boundNodeIDVector->isNull(boundSelVector[i]));
                    DASSERT(boundNodeIDVector->getValue<nodeID_t>(boundSelVector[i]).tableID ==
                            firstTableID);
                }
            }
        });
        const auto currentIdx = boundNodeIDVector->state->getSelVector()[0];
        if (boundNodeIDVector->isNull(currentIdx)) {
            currentScanner = nullptr;
            continue;
        }
        auto nodeID = boundNodeIDVector->getValue<nodeID_t>(currentIdx);
        initCurrentScanner(nodeID);
    }
}

void ScanMultiRelTable::resetState() {
    currentScanner = nullptr;
    for (auto& [_, scanner] : scanners) {
        scanner.resetState();
    }
}

void ScanMultiRelTable::initCurrentScanner(const nodeID_t& nodeID) {
    if (scanners.contains(nodeID.tableID)) {
        currentScanner = &scanners.at(nodeID.tableID);
        currentScanner->resetState();
    } else {
        currentScanner = nullptr;
    }
}

} // namespace processor
} // namespace riag

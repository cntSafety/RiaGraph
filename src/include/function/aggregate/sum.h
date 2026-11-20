#pragma once

#include "function/aggregate_function.h"
#include "function/arithmetic/add.h"

namespace riag {
namespace function {

// openCypher gives sum an identity element: over no input, or over input that is all NULL, the
// result is the argument's zero, not NULL (avg, min and max stay NULL). The state therefore starts
// as a non-NULL zero, for the simple and the hash aggregate alike, and merging an untouched
// partial state in combine adds zero.
template<typename RESULT_TYPE>
struct SumState : public AggregateStateWithNull {
    SumState() { isNull = false; }

    uint32_t getStateSize() const override { return sizeof(*this); }
    void writeToVector(common::ValueVector* outputVector, uint64_t pos) override {
        outputVector->setValue(pos, sum);
    }

    RESULT_TYPE sum{};
};

template<typename INPUT_TYPE, typename RESULT_TYPE>
struct SumFunction {
    static std::unique_ptr<AggregateState> initialize() {
        return std::make_unique<SumState<RESULT_TYPE>>();
    }

    static void updateAll(uint8_t* state_, common::ValueVector* input, uint64_t multiplicity,
        common::InMemOverflowBuffer* /*overflowBuffer*/) {
        DASSERT(!input->state->isFlat());
        auto* state = reinterpret_cast<SumState<RESULT_TYPE>*>(state_);
        input->forEachNonNull(
            [&](auto pos) { updateSingleValue(state, input, pos, multiplicity); });
    }

    static void updatePos(uint8_t* state_, common::ValueVector* input, uint64_t multiplicity,
        uint32_t pos, common::InMemOverflowBuffer* /*overflowBuffer*/) {
        auto* state = reinterpret_cast<SumState<RESULT_TYPE>*>(state_);
        updateSingleValue(state, input, pos, multiplicity);
    }

    static void updateSingleValue(SumState<RESULT_TYPE>* state, common::ValueVector* input,
        uint32_t pos, uint64_t multiplicity) {
        INPUT_TYPE val = input->getValue<INPUT_TYPE>(pos);
        for (auto j = 0u; j < multiplicity; ++j) {
            Add::operation(state->sum, val, state->sum);
        }
    }

    static void combine(uint8_t* state_, uint8_t* otherState_,
        common::InMemOverflowBuffer* /*overflowBuffer*/) {
        auto* otherState = reinterpret_cast<SumState<RESULT_TYPE>*>(otherState_);
        auto* state = reinterpret_cast<SumState<RESULT_TYPE>*>(state_);
        Add::operation(state->sum, otherState->sum, state->sum);
    }

    static void finalize(uint8_t* /*state_*/) {}
};

} // namespace function
} // namespace riag

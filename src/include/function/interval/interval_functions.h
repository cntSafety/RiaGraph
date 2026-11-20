#pragma once

#include "common/exception/overflow.h"
#include "common/types/interval_t.h"
#include "function/arithmetic/multiply.h"
#include <format>

namespace riag {
namespace function {

// The to_<unit> functions take an INT64 and fill the INT32 months/days fields or the INT64 micros
// field. The scaling is checked, and so is the narrowing to INT32: both used to wrap silently.
struct IntervalFieldCast {
    // The factor and input are copied into non-const lvalues: the checked Multiply specialization
    // only matches plain int64_t&, a const operand silently selects the unchecked template.
    static inline int64_t scale(int64_t input, int64_t factor) {
        int64_t result = 0;
        Multiply::operation(input, factor, result);
        return result;
    }

    static inline int32_t toInt32(int64_t value) {
        if (value < INT32_MIN || value > INT32_MAX) {
            throw common::OverflowException(
                std::format("Value {} is not within INT32 range.", value));
        }
        return static_cast<int32_t>(value);
    }
};

struct ToYears {
    static inline void operation(int64_t& input, common::interval_t& result) {
        result.days = result.micros = 0;
        int64_t months = IntervalFieldCast::scale(input, common::Interval::MONTHS_PER_YEAR);
        result.months = IntervalFieldCast::toInt32(months);
    }
};

struct ToMonths {
    static inline void operation(int64_t& input, common::interval_t& result) {
        result.days = result.micros = 0;
        result.months = IntervalFieldCast::toInt32(input);
    }
};

struct ToDays {
    static inline void operation(int64_t& input, common::interval_t& result) {
        result.micros = result.months = 0;
        result.days = IntervalFieldCast::toInt32(input);
    }
};

struct ToHours {
    static inline void operation(int64_t& input, common::interval_t& result) {
        result.months = result.days = 0;
        result.micros = IntervalFieldCast::scale(input, common::Interval::MICROS_PER_HOUR);
    }
};

struct ToMinutes {
    static inline void operation(int64_t& input, common::interval_t& result) {
        result.months = result.days = 0;
        result.micros = IntervalFieldCast::scale(input, common::Interval::MICROS_PER_MINUTE);
    }
};

struct ToSeconds {
    static inline void operation(int64_t& input, common::interval_t& result) {
        result.months = result.days = 0;
        result.micros = IntervalFieldCast::scale(input, common::Interval::MICROS_PER_SEC);
    }
};

struct ToMilliseconds {
    static inline void operation(int64_t& input, common::interval_t& result) {
        result.months = result.days = 0;
        result.micros = IntervalFieldCast::scale(input, common::Interval::MICROS_PER_MSEC);
    }
};

struct ToMicroseconds {
    static inline void operation(int64_t& input, common::interval_t& result) {
        result.months = result.days = 0;
        result.micros = input;
    }
};

} // namespace function
} // namespace riag

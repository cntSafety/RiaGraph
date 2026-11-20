#pragma once

#include <cmath>
#include <string>
#include <type_traits>

#include "common/exception/overflow.h"
#include "common/type_utils.h"
#include "common/types/int128_t.h"
#include "common/types/types.h"
#include "common/vector/value_vector.h"
#include "function/cast/functions/cast_string_non_nested_functions.h"
#include "function/cast/functions/numeric_limits.h"
#include <format>

namespace riag {
namespace function {

template<typename A, typename B>
struct pickDecimalPhysicalType {
    static constexpr bool AISFLOAT = std::is_floating_point<A>::value;
    static constexpr bool BISFLOAT = std::is_floating_point<B>::value;
    using RES =
        typename std::conditional<(AISFLOAT ? false : (BISFLOAT ? true : sizeof(A) > sizeof(B))), A,
            B>::type;
};

struct CastDecimalTo {
    template<typename SRC, typename DST>
    static void operation(SRC& input, DST& output, const ValueVector& inputVec,
        ValueVector& outputVec) {
        auto scale = DecimalType::getScale(inputVec.dataType);
        if constexpr (std::is_floating_point<DST>::value) {
            using T = typename pickDecimalPhysicalType<SRC, DST>::RES;
            constexpr auto pow10s = pow10Sequence<T>();
            output = (DST)input / (DST)pow10s[scale];
        } else {
            // The integral part is computed in SRC, the DECIMAL's own (signed) physical type: the
            // wider of SRC and DST would be unsigned for an unsigned DST of the same size, and
            // then the quotient of a negative value wraps and no minimum check can see it.
            // |tmp| <= |input| after the division, and the rounding term cannot overflow SRC
            // since |input| < 10^precision.
            constexpr auto pow10s = pow10Sequence<SRC>();
            SRC tmp = static_cast<SRC>(
                ((scale > 0 ? pow10s[scale - 1] * (input < 0 ? -5 : 5) : 0) + input) /
                pow10s[scale]);
            // Compared in SRC, which is signed, against limits of DST that are cast to SRC only
            // where DST is the narrower type, so neither side converts to the other's sign.
            bool outOfRange = false;
            if constexpr (!NumericLimits<DST>::isSigned()) {
                outOfRange = tmp < 0;
            } else if constexpr (sizeof(DST) < sizeof(SRC)) {
                outOfRange = tmp < static_cast<SRC>(NumericLimits<DST>::minimum());
            }
            if constexpr (sizeof(DST) < sizeof(SRC)) {
                outOfRange = outOfRange || tmp > static_cast<SRC>(NumericLimits<DST>::maximum());
            }
            if (outOfRange) {
                throw OverflowException(std::format("Cast Failed: {} is not in {} range",
                    DecimalType::insertDecimalPoint(TypeUtils::toString(input), scale),
                    outputVec.dataType.toString()));
            }
            output = static_cast<DST>(tmp);
        }
    }
};

struct CastToDecimal {
    // The range is checked on the source value, before it is scaled and narrowed to the
    // DECIMAL's physical type: a value that does not fit would otherwise wrap, or be undefined
    // behaviour for a floating source, and the wrapped result could land back inside the range.
    template<typename SRC, typename DST>
    static void operation(SRC& input, DST& output, const ValueVector&,
        const ValueVector& outputVec) {
        constexpr auto pow10s = pow10Sequence<DST>();
        auto precision = DecimalType::getPrecision(outputVec.dataType);
        auto scale = DecimalType::getScale(outputVec.dataType);
        if constexpr (std::is_floating_point<SRC>::value) {
            // NaN and infinity compare false against everything, so name them.
            if (std::isnan(input) || std::isinf(input)) {
                throwOutOfRange(input, outputVec);
            }
            auto roundconst = (input < 0 ? -0.5 : 0.5);
            double rounded = (double)pow10s[scale] * input + roundconst;
            double limit = (double)pow10s[precision];
            if (!(rounded > -limit && rounded < limit)) {
                throwOutOfRange(input, outputVec);
            }
            // Truncate explicitly: int128_t's conversion from double rounds to nearest, which
            // would round a second time on top of roundconst.
            output = (DST)std::trunc(rounded);
            // The limit above is exact only up to 2^53; the converted value gets the final say.
            if (output <= -pow10s[precision] || output >= pow10s[precision]) {
                throwOutOfRange(input, outputVec);
            }
        } else {
            // |input * 10^scale| < 10^precision  <=>  |input| < 10^(precision - scale).
            if constexpr (common::UnsignedIntegerTypes<SRC>) {
                if (common::uint128_t(input) >=
                    pow10Sequence<common::uint128_t>()[precision - scale]) {
                    throwOutOfRange(input, outputVec);
                }
            } else {
                common::int128_t limit = pow10Sequence<common::int128_t>()[precision - scale];
                if (common::int128_t(input) <= -limit || common::int128_t(input) >= limit) {
                    throwOutOfRange(input, outputVec);
                }
            }
            // In range, so neither the narrowing nor the product can overflow DST.
            output = (DST)(pow10s[scale] * static_cast<DST>(input));
        }
    }

private:
    template<typename SRC>
    [[noreturn]] static void throwOutOfRange(const SRC& input, const ValueVector& outputVec) {
        throw OverflowException(std::format("To Decimal Cast Failed: {} is not in {} range",
            TypeUtils::toString(input), outputVec.dataType.toString()));
    }
};

struct CastBetweenDecimal {
    template<typename SRC, typename DST>
    static void operation(SRC& input, DST& output, const ValueVector& inputVec,
        const ValueVector& outputVec) {
        // Both sides are DECIMAL physical types, which are signed: the range checks below
        // compare a signed input against T and negate T's limits.
        static_assert(NumericLimits<SRC>::isSigned() && NumericLimits<DST>::isSigned());
        using T = typename pickDecimalPhysicalType<SRC, DST>::RES;
        constexpr auto pow10s = pow10Sequence<T>();
        auto outputPrecision = DecimalType::getPrecision(outputVec.dataType);
        auto inputScale = DecimalType::getScale(inputVec.dataType);
        auto outputScale = DecimalType::getScale(outputVec.dataType);
        // Scale and range-check in T, the wider of the two physical types, and narrow to DST
        // only once the value is known to fit: narrowing first would wrap it.
        T scaled;
        if (inputScale == outputScale) {
            scaled = static_cast<T>(input);
        } else if (inputScale < outputScale) {
            auto shift = outputScale - inputScale;
            // Checked on the input, since input * 10^shift may itself overflow T. shift <=
            // outputScale <= outputPrecision.
            auto limit = pow10s[outputPrecision - shift];
            if (input >= limit || input <= -limit) {
                throwOutOfRange(input, inputVec, outputVec);
            }
            scaled = static_cast<T>(pow10s[shift] * input);
        } else {
            auto roundconst = (input < 0 ? -5 : 5);
            scaled = static_cast<T>((pow10s[inputScale - outputScale - 1] * roundconst + input) /
                                    pow10s[inputScale - outputScale]);
        }
        if (pow10s[outputPrecision] <= scaled || -pow10s[outputPrecision] >= scaled) {
            throwOutOfRange(input, inputVec, outputVec);
        }
        output = static_cast<DST>(scaled);
    }

private:
    template<typename SRC>
    [[noreturn]] static void throwOutOfRange(const SRC& input, const ValueVector& inputVec,
        const ValueVector& outputVec) {
        throw OverflowException(std::format("Decimal Cast Failed: input {} is not in range of {}",
            DecimalType::insertDecimalPoint(TypeUtils::toString(input, nullptr),
                DecimalType::getScale(inputVec.dataType)),
            outputVec.dataType.toString()));
    }
};

// DECIMAL TO STRING SPECIALIZATION
template<>
inline void CastDecimalTo::operation(int16_t& input, string_t& output, const ValueVector& inputVec,
    ValueVector& resultVector) {
    auto scale = DecimalType::getScale(inputVec.dataType);
    auto str = DecimalType::insertDecimalPoint(std::to_string(input), scale);
    common::StringVector::addString(&resultVector, output, str);
}

template<>
inline void CastDecimalTo::operation(int32_t& input, string_t& output, const ValueVector& inputVec,
    ValueVector& resultVector) {
    auto scale = DecimalType::getScale(inputVec.dataType);
    auto str = DecimalType::insertDecimalPoint(std::to_string(input), scale);
    common::StringVector::addString(&resultVector, output, str);
}

template<>
inline void CastDecimalTo::operation(int64_t& input, string_t& output, const ValueVector& inputVec,
    ValueVector& resultVector) {
    auto scale = DecimalType::getScale(inputVec.dataType);
    auto str = DecimalType::insertDecimalPoint(std::to_string(input), scale);
    common::StringVector::addString(&resultVector, output, str);
}

template<>
inline void CastDecimalTo::operation(common::int128_t& input, string_t& output,
    const ValueVector& inputVec, ValueVector& resultVector) {
    auto scale = DecimalType::getScale(inputVec.dataType);
    auto str = DecimalType::insertDecimalPoint(common::Int128_t::toString(input), scale);
    common::StringVector::addString(&resultVector, output, str);
}

// STRING TO DECIMAL SPECIALIZATION
template<>
inline void CastToDecimal::operation(string_t& input, int16_t& output, const ValueVector&,
    const ValueVector& outputVec) {
    decimalCast((const char*)input.getData(), input.len, output, outputVec.dataType);
}

template<>
inline void CastToDecimal::operation(string_t& input, int32_t& output, const ValueVector&,
    const ValueVector& outputVec) {
    decimalCast((const char*)input.getData(), input.len, output, outputVec.dataType);
}

template<>
inline void CastToDecimal::operation(string_t& input, int64_t& output, const ValueVector&,
    const ValueVector& outputVec) {
    decimalCast((const char*)input.getData(), input.len, output, outputVec.dataType);
}

template<>
inline void CastToDecimal::operation(string_t& input, common::int128_t& output, const ValueVector&,
    const ValueVector& outputVec) {
    decimalCast((const char*)input.getData(), input.len, output, outputVec.dataType);
}

} // namespace function
} // namespace riag

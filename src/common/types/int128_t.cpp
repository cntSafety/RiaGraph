#include "common/types/int128_t.h"

#include <cmath>
#include <cstdint>
#include <cstring>

#include "common/exception/runtime.h"
#include "common/numeric_utils.h"
#include "common/type_utils.h"
#include "common/types/uint128_t.h"
#include "function/cast/functions/numeric_limits.h"
#include "function/hash/hash_functions.h"
#include <bit>
#include <format>

namespace riag::common {

static uint8_t positiveInt128BitsAmount(int128_t input) {
    if (input.high) {
        return 128 - std::countl_zero((uint64_t)input.high);
    } else {
        return 64 - std::countl_zero(input.low);
    }
}

static bool positiveInt128IsBitSet(int128_t input, uint8_t bit) {
    if (bit < 64) {
        return input.low & (1ULL << uint64_t(bit));
    } else {
        return input.high & (1ULL << uint64_t(bit - 64));
    }
}

int128_t positiveInt128LeftShift(int128_t lhs, uint32_t amount) {
    int128_t result{};
    result.low = lhs.low << amount;
    result.high = (lhs.high << amount) + (lhs.low >> (64 - amount));
    return result;
}

int128_t Int128_t::divModPositive(int128_t lhs, uint64_t rhs, uint64_t& remainder) {
    int128_t result{0};
    remainder = 0;

    for (uint8_t i = positiveInt128BitsAmount(lhs); i > 0; i--) {
        result = positiveInt128LeftShift(result, 1);
        remainder <<= 1;
        if (positiveInt128IsBitSet(lhs, i - 1)) {
            remainder++;
        }
        if (remainder >= rhs) {
            remainder -= rhs;
            result.low++;
            if (result.low == 0) {
                result.high++;
            }
        }
    }
    return result;
}

std::string Int128_t::toString(int128_t input) {
    bool isMin = (input.high == INT64_MIN && input.low == 0);
    bool negative = input.high < 0;

    if (isMin) {

        uint64_t remainder = 0;
        int128_t quotient = divModPositive(input, 10, remainder);

        std::string result = toString(quotient);

        result += static_cast<char>('0' + remainder);

        return "-" + result;
    }

    if (negative) {
        negateInPlace(input);
    }

    std::string result;
    uint64_t remainder = 0;

    while (input.high != 0 || input.low != 0) {
        input = divModPositive(input, 10, remainder);
        result = std::string(1, '0' + remainder) + std::move(result);
    }

    if (result.empty()) {
        result = "0";
    }

    return negative ? "-" + result : result;
}

// Portable overflow-checked int64 addition/subtraction with a carry/borrow.
// MSVC does not support __int128, and a naive `a + b + carry` in int64 can
// overflow the intermediate (e.g. INT64_MIN + -1 + 1) even when the final
// result fits, tripping UBSan. The helpers below use only well-defined
// unsigned wrapping arithmetic plus sign analysis, so they are safe on all
// compilers (MSVC/GCC/Clang) and under UBSan.
//
// For addition, write S = a + b + carry with ua/ub the 2's-complement bit
// patterns. Let U = ua + ub + carry (0..2^65-1), carry2 = U / 2^64 in {0,1},
// k = (a<0) + (b<0), and usum = U mod 2^64. Then S = (carry2-k)*2^64 + usum.
// With t = carry2-k in {-2..1}: t==1/t==-2 always overflows, t==0 needs sign
// bit 0, t==-1 needs sign bit 1. Subtraction is analogous with
// S = a - b - borrow = Dmod + (kb-ka-borrow2)*2^64.
static bool addInt64WithCarry(int64_t a, int64_t b, int carry, int64_t& out) {
    uint64_t ua = 0;
    uint64_t ub = 0;
    static_assert(sizeof(ua) == sizeof(a));
    std::memcpy(&ua, &a, sizeof(a));
    std::memcpy(&ub, &b, sizeof(b));
    uint64_t tmp = ua + ub;
    int c1 = (tmp < ua) ? 1 : 0;
    uint64_t usum = tmp + static_cast<uint64_t>(carry);
    int c2 = (usum < tmp) ? 1 : 0;
    int carry2 = c1 + c2;
    int k = (a < 0 ? 1 : 0) + (b < 0 ? 1 : 0);
    int t = carry2 - k;
    bool sign = (usum >> 63) != 0;
    bool overflow = false;
    if (t == 1 || t == -2) {
        overflow = true;
    } else if (t == 0) {
        overflow = sign;
    } else { // t == -1
        overflow = !sign;
    }
    if (overflow) {
        return false;
    }
    std::memcpy(&out, &usum, sizeof(out));
    return true;
}

static bool subInt64WithBorrow(int64_t a, int64_t b, int borrow, int64_t& out) {
    uint64_t ua = 0;
    uint64_t ub = 0;
    static_assert(sizeof(ua) == sizeof(a));
    std::memcpy(&ua, &a, sizeof(a));
    std::memcpy(&ub, &b, sizeof(b));
    uint64_t tmp = ua - ub;
    int b1 = (ua < ub) ? 1 : 0;
    uint64_t udiff = tmp - static_cast<uint64_t>(borrow);
    int b2 = (tmp < static_cast<uint64_t>(borrow)) ? 1 : 0;
    int borrow2 = b1 + b2;
    int t = (b < 0 ? 1 : 0) - (a < 0 ? 1 : 0) - borrow2;
    bool sign = (udiff >> 63) != 0;
    bool overflow = false;
    if (t == 1 || t == -2) {
        overflow = true;
    } else if (t == 0) {
        overflow = sign;
    } else { // t == -1
        overflow = !sign;
    }
    if (overflow) {
        return false;
    }
    std::memcpy(&out, &udiff, sizeof(out));
    return true;
}

// The operations below that need an absolute value work on magnitudes: the bit pattern of
// |value| as an unsigned 128-bit number. The magnitude of INT128_MIN is 2^127, which has no
// int128_t, so it cannot be formed with negateInPlace; unsigned wrapping arithmetic has no such
// gap and, unlike negating the signed halves, no signed overflow.
struct Magnitude {
    uint64_t low;
    uint64_t high;
};

static Magnitude negateMagnitude(Magnitude value) {
    // Two's complement: ~value + 1.
    Magnitude result{};
    result.low = uint64_t(0) - value.low;
    result.high = ~value.high + (result.low == 0 ? 1 : 0);
    return result;
}

static Magnitude toMagnitude(int128_t value, bool& negative) {
    negative = value.high < 0;
    Magnitude result{value.low, static_cast<uint64_t>(value.high)};
    return negative ? negateMagnitude(result) : result;
}

// Whether the signed value with this sign and magnitude is representable: at most 2^127 - 1 if
// positive and at most 2^127 if negative.
static bool magnitudeFits(Magnitude value, bool negative) {
    constexpr uint64_t signBit = uint64_t(1) << 63;
    if (value.high < signBit) {
        return true;
    }
    return negative && value.high == signBit && value.low == 0;
}

// Precondition: magnitudeFits(value, negative).
static int128_t fromMagnitude(Magnitude value, bool negative) {
    if (negative) {
        value = negateMagnitude(value);
    }
    return int128_t{value.low, static_cast<int64_t>(value.high)};
}

// Full 64 x 64 -> 128 bit product from 32-bit limbs, which needs no compiler support.
static void multiplyFull(uint64_t lhs, uint64_t rhs, uint64_t& high, uint64_t& low) {
    uint64_t lhsLow = lhs & 0xffffffff;
    uint64_t lhsHigh = lhs >> 32;
    uint64_t rhsLow = rhs & 0xffffffff;
    uint64_t rhsHigh = rhs >> 32;
    uint64_t lowLow = lhsLow * rhsLow;
    uint64_t lowHigh = lhsLow * rhsHigh;
    uint64_t highLow = lhsHigh * rhsLow;
    uint64_t highHigh = lhsHigh * rhsHigh;
    // At most 3 * (2^32 - 1): no overflow.
    uint64_t middle = (lowLow >> 32) + (lowHigh & 0xffffffff) + (highLow & 0xffffffff);
    low = (lowLow & 0xffffffff) | (middle << 32);
    high = highHigh + (lowHigh >> 32) + (highLow >> 32) + (middle >> 32);
}

// lhs * rhs as unsigned 128-bit numbers; false if the product needs more than 128 bits.
[[maybe_unused]] static bool multiplyMagnitudePortable(Magnitude lhs, Magnitude rhs,
    Magnitude& result) {
    // With lhs = lhs.high * 2^64 + lhs.low and the same for rhs, both high parts non-zero means
    // a product of at least 2^128.
    if (lhs.high != 0 && rhs.high != 0) {
        return false;
    }
    uint64_t high = 0;
    uint64_t low = 0;
    multiplyFull(lhs.low, rhs.low, high, low);
    // The product of a high part and a low part is shifted by 64 bits: it has to fit in 64.
    uint64_t crossHigh = 0;
    uint64_t crossLow = 0;
    if (lhs.high != 0) {
        multiplyFull(lhs.high, rhs.low, crossHigh, crossLow);
    } else if (rhs.high != 0) {
        multiplyFull(lhs.low, rhs.high, crossHigh, crossLow);
    }
    if (crossHigh != 0 || high + crossLow < high) {
        return false;
    }
    result.low = low;
    result.high = high + crossLow;
    return true;
}

static bool multiplyMagnitude(Magnitude lhs, Magnitude rhs, Magnitude& result) {
#if ((__GNUC__ >= 5) || defined(__clang__)) && defined(__SIZEOF_INT128__)
    __uint128_t left = __uint128_t(lhs.low) + (__uint128_t(lhs.high) << 64);
    __uint128_t right = __uint128_t(rhs.low) + (__uint128_t(rhs.high) << 64);
    __uint128_t product = 0;
    if (__builtin_mul_overflow(left, right, &product)) {
        return false;
    }
    result.low = uint64_t(product & 0xffffffffffffffff);
    result.high = uint64_t(product >> 64);
    return true;
#else
    return multiplyMagnitudePortable(lhs, rhs, result);
#endif
}

static uint32_t magnitudeBitsAmount(Magnitude value) {
    if (value.high != 0) {
        return 128 - std::countl_zero(value.high);
    }
    return 64 - std::countl_zero(value.low);
}

static bool magnitudeGreaterThanOrEquals(Magnitude lhs, Magnitude rhs) {
    return lhs.high > rhs.high || (lhs.high == rhs.high && lhs.low >= rhs.low);
}

// Schoolbook shift-subtract division of unsigned 128-bit numbers. The remainder stays below
// rhs <= 2^127 and so does not lose a bit when it is shifted. Precondition: rhs != 0.
static void divModMagnitude(Magnitude lhs, Magnitude rhs, Magnitude& quotient,
    Magnitude& remainder) {
    quotient = Magnitude{0, 0};
    remainder = Magnitude{0, 0};
    for (uint32_t bit = magnitudeBitsAmount(lhs); bit > 0; bit--) {
        quotient.high = (quotient.high << 1) | (quotient.low >> 63);
        quotient.low <<= 1;
        remainder.high = (remainder.high << 1) | (remainder.low >> 63);
        remainder.low <<= 1;
        uint64_t lhsBit = bit - 1 < 64 ? (lhs.low >> (bit - 1)) & 1 : (lhs.high >> (bit - 65)) & 1;
        remainder.low |= lhsBit;
        if (magnitudeGreaterThanOrEquals(remainder, rhs)) {
            uint64_t borrow = remainder.low < rhs.low ? 1 : 0;
            remainder.low -= rhs.low;
            remainder.high -= rhs.high + borrow;
            quotient.low |= 1;
        }
    }
}

bool Int128_t::addInPlace(int128_t& lhs, int128_t rhs) {
    bool lhsPositive = lhs.high >= 0;
    bool rhsPositive = rhs.high >= 0;
    // low is unsigned, so wrapping addition and the carry check are well-defined.
    int carry = (lhs.low + rhs.low < lhs.low) ? 1 : 0;
    int64_t highSum = 0;
    if (!addInt64WithCarry(lhs.high, rhs.high, carry, highSum)) {
        return false;
    }
    lhs.high = highSum;
    lhs.low += rhs.low;
    if (lhsPositive && rhsPositive && lhs.high == INT64_MIN && lhs.low == 0) {
        return false;
    }
    return true;
}

bool Int128_t::subInPlace(int128_t& lhs, int128_t rhs) {
    // low is unsigned, so wrapping subtraction and the borrow check are well-defined.
    int borrow = (lhs.low - rhs.low > lhs.low) ? 1 : 0;
    int64_t highDiff = 0;
    if (!subInt64WithBorrow(lhs.high, rhs.high, borrow, highDiff)) {
        return false;
    }
    // The difference of the high parts, with the borrow, is within int64 exactly when the
    // 128-bit difference is within int128, INT128_MIN included.
    lhs.high = highDiff;
    lhs.low -= rhs.low;
    return true;
}

int128_t Int128_t::Add(int128_t lhs, const int128_t rhs) {
    if (!addInPlace(lhs, rhs)) {
        throw common::OverflowException("INT128 is out of range: cannot add.");
    }
    return lhs;
}

int128_t Int128_t::Sub(int128_t lhs, const int128_t rhs) {
    if (!subInPlace(lhs, rhs)) {
        throw common::OverflowException("INT128 is out of range: cannot subtract.");
    }
    return lhs;
}

bool Int128_t::tryMultiply(int128_t lhs, int128_t rhs, int128_t& result) {
    bool lhsNegative = false;
    bool rhsNegative = false;
    Magnitude product{};
    if (!multiplyMagnitude(toMagnitude(lhs, lhsNegative), toMagnitude(rhs, rhsNegative), product)) {
        return false;
    }
    // A zero product is representable with either sign.
    bool negative = lhsNegative != rhsNegative;
    if (!magnitudeFits(product, negative)) {
        return false;
    }
    result = fromMagnitude(product, negative);
    return true;
}

int128_t Int128_t::Mul(int128_t lhs, int128_t rhs) {
    int128_t result{};
    if (!tryMultiply(lhs, rhs, result)) {
        throw common::OverflowException("INT128 is out of range: cannot multiply.");
    }
    return result;
}

// The remainder takes the sign of lhs, the quotient is negative if the signs differ. The
// quotient is returned as a magnitude because INT128_MIN / -1 = 2^127 has no int128_t; the
// remainder is always smaller than |rhs| and fits.
static void divModSigned(int128_t lhs, int128_t rhs, Magnitude& quotient, bool& quotientNegative,
    int128_t& remainder) {
    bool lhsNegative = false;
    bool rhsNegative = false;
    Magnitude remainderMagnitude{};
    divModMagnitude(toMagnitude(lhs, lhsNegative), toMagnitude(rhs, rhsNegative), quotient,
        remainderMagnitude);
    quotientNegative = lhsNegative != rhsNegative;
    remainder = fromMagnitude(remainderMagnitude, lhsNegative);
}

int128_t Int128_t::divMod(int128_t lhs, int128_t rhs, int128_t& remainder) {
    Magnitude quotient{};
    bool quotientNegative = false;
    divModSigned(lhs, rhs, quotient, quotientNegative, remainder);
    if (!magnitudeFits(quotient, quotientNegative)) {
        // Only INT128_MIN / -1.
        throw common::OverflowException("INT128 is out of range: cannot divide.");
    }
    return fromMagnitude(quotient, quotientNegative);
}

int128_t Int128_t::Div(int128_t lhs, int128_t rhs) {
    if (rhs.high == 0 && rhs.low == 0) {
        throw common::RuntimeException("Divide by zero.");
    }
    int128_t remainder{};
    return divMod(lhs, rhs, remainder);
}

int128_t Int128_t::Mod(int128_t lhs, int128_t rhs) {
    if (rhs.high == 0 && rhs.low == 0) {
        throw common::RuntimeException("Modulo by zero.");
    }
    // The remainder of INT128_MIN % -1 is 0, so unlike Div this must not look at the quotient.
    Magnitude quotient{};
    bool quotientNegative = false;
    int128_t remainder{};
    divModSigned(lhs, rhs, quotient, quotientNegative, remainder);
    return remainder;
}

int128_t Int128_t::Xor(int128_t lhs, int128_t rhs) {
    int128_t result{lhs.low ^ rhs.low, lhs.high ^ rhs.high};
    return result;
}

int128_t Int128_t::BinaryAnd(int128_t lhs, int128_t rhs) {
    int128_t result{lhs.low & rhs.low, lhs.high & rhs.high};
    return result;
}

int128_t Int128_t::BinaryOr(int128_t lhs, int128_t rhs) {
    int128_t result{lhs.low | rhs.low, lhs.high | rhs.high};
    return result;
}

int128_t Int128_t::BinaryNot(int128_t val) {
    return int128_t{~val.low, ~val.high};
}

int128_t Int128_t::LeftShift(int128_t lhs, int amount) {
    // adapted from
    // https://github.com/abseil/abseil-cpp/blob/master/absl/numeric/int128.h
    return amount >= 64 ? int128_t(0, lhs.low << (amount - 64)) :
           amount == 0  ? lhs :
                          int128_t{lhs.low << amount,
                             (lhs.high << amount) |
                                 (numeric_utils::makeValueSigned(lhs.low >> (64 - amount)))};
}

int128_t Int128_t::RightShift(int128_t lhs, int amount) {
    // adapted from
    // https://github.com/abseil/abseil-cpp/blob/master/absl/numeric/int128.h
    return amount >= 64 ?
               // we shift the high value regardless for sign extension
               int128_t(lhs.high >> (amount - 64), lhs.high >> 63) :
               amount == 0 ?
               lhs :
               int128_t((lhs.low >> amount) | (lhs.high << (64 - amount)), lhs.high >> amount);
}

//===============================================================================================
// Cast operation
//===============================================================================================
template<class DST, bool SIGNED = true>
bool TryCastInt128Template(int128_t input, DST& result) {
    switch (input.high) {
    case 0:
        if (input.low <= uint64_t(function::NumericLimits<DST>::maximum())) {
            result = static_cast<DST>(input.low);
            return true;
        }
        break;
    case -1:
        if constexpr (!SIGNED) {
            throw common::OverflowException(
                "Cast failed. Cannot cast " + Int128_t::toString(input) + " to unsigned type.");
        }
        if (input.low >= function::NumericLimits<uint64_t>::maximum() -
                             uint64_t(function::NumericLimits<DST>::maximum())) {
            result = -DST(function::NumericLimits<uint64_t>::maximum() - input.low) - 1;
            return true;
        }
        break;
    default:
        break;
    }
    return false;
}
// we can use the above template if we can get max using something like DST.max

template<>
bool Int128_t::tryCast(int128_t input, int8_t& result) {
    return TryCastInt128Template<int8_t>(input, result);
}

template<>
bool Int128_t::tryCast(int128_t input, int16_t& result) {
    return TryCastInt128Template<int16_t>(input, result);
}

template<>
bool Int128_t::tryCast(int128_t input, int32_t& result) {
    return TryCastInt128Template<int32_t>(input, result);
}

template<>
bool Int128_t::tryCast(int128_t input, int64_t& result) {
    return TryCastInt128Template<int64_t>(input, result);
}

template<>
bool Int128_t::tryCast(int128_t input, uint8_t& result) {
    return TryCastInt128Template<uint8_t, false>(input, result);
}

template<>
bool Int128_t::tryCast(int128_t input, uint16_t& result) {
    return TryCastInt128Template<uint16_t, false>(input, result);
}

template<>
bool Int128_t::tryCast(int128_t input, uint32_t& result) {
    return TryCastInt128Template<uint32_t, false>(input, result);
}

template<>
bool Int128_t::tryCast(int128_t input, uint64_t& result) {
    return TryCastInt128Template<uint64_t, false>(input, result);
}

template<>
bool Int128_t::tryCast(int128_t input, uint128_t& result) {
    if (input.high < 0) {
        return false;
    }
    result.low = input.low;
    result.high = uint64_t(input.high);
    return true;
}

template<>
bool Int128_t::tryCast(int128_t input, float& result) {
    double temp_res = NAN;
    tryCast(input, temp_res);
    result = static_cast<float>(temp_res);
    return true;
}

template<class REAL_T>
bool CastInt128ToFloating(int128_t input, REAL_T& result) {
    switch (input.high) {
    case -1:
        result = -REAL_T(function::NumericLimits<uint64_t>::maximum() - input.low) - 1;
        break;
    default:
        result = REAL_T(input.high) * REAL_T(function::NumericLimits<uint64_t>::maximum()) +
                 REAL_T(input.low);
        break;
    }
    return true;
}

template<>
bool Int128_t::tryCast(int128_t input, double& result) {
    return CastInt128ToFloating<double>(input, result);
}

template<>
bool Int128_t::tryCast(int128_t input, long double& result) {
    return CastInt128ToFloating<long double>(input, result);
}

template<class SRC>
int128_t tryCastToTemplate(SRC value) {
    int128_t result{};
    result.low = (uint64_t)value;
    result.high = (value < 0) * -1;
    return result;
}

template<>
bool Int128_t::tryCastTo(int8_t value, int128_t& result) {
    result = tryCastToTemplate<int8_t>(value);
    return true;
}

template<>
bool Int128_t::tryCastTo(int16_t value, int128_t& result) {
    result = tryCastToTemplate<int16_t>(value);
    return true;
}

template<>
bool Int128_t::tryCastTo(int32_t value, int128_t& result) {
    result = tryCastToTemplate<int32_t>(value);
    return true;
}

template<>
bool Int128_t::tryCastTo(int64_t value, int128_t& result) {
    result = tryCastToTemplate<int64_t>(value);
    return true;
}

template<>
bool Int128_t::tryCastTo(uint8_t value, int128_t& result) {
    result = tryCastToTemplate<uint8_t>(value);
    return true;
}

template<>
bool Int128_t::tryCastTo(uint16_t value, int128_t& result) {
    result = tryCastToTemplate<uint16_t>(value);
    return true;
}

template<>
bool Int128_t::tryCastTo(uint32_t value, int128_t& result) {
    result = tryCastToTemplate<uint32_t>(value);
    return true;
}

template<>
bool Int128_t::tryCastTo(uint64_t value, int128_t& result) {
    result = tryCastToTemplate<uint64_t>(value);
    return true;
}

template<>
bool Int128_t::tryCastTo(int128_t value, int128_t& result) {
    result = value;
    return true;
}

template<>
bool Int128_t::tryCastTo(float value, int128_t& result) {
    return tryCastTo(double(value), result);
}

template<class REAL_T>
bool castFloatingToInt128(REAL_T value, int128_t& result) {
    // TODO: Maybe need to add func isFinite in value.h to see if every type is finite.
    if (value <= -170141183460469231731687303715884105728.0 ||
        value >= 170141183460469231731687303715884105727.0) {
        return false;
    }
    bool negative = value < 0;
    if (negative) {
        value = -value;
    }
    value = std::nearbyint(value);
    result.low = (uint64_t)fmod(value, REAL_T(function::NumericLimits<uint64_t>::maximum()));
    result.high = (uint64_t)(value / REAL_T(function::NumericLimits<uint64_t>::maximum()));
    if (negative) {
        Int128_t::negateInPlace(result);
    }
    return true;
}

template<>
bool Int128_t::tryCastTo(double value, int128_t& result) {
    return castFloatingToInt128<double>(value, result);
}

template<>
bool Int128_t::tryCastTo(long double value, int128_t& result) {
    return castFloatingToInt128<long double>(value, result);
}
//===============================================================================================

template<NumericTypes T>
void constructInt128Template(T value, int128_t& result) {
    int128_t casted = Int128_t::castTo(value);
    result.low = casted.low;
    result.high = casted.high;
}

int128_t::int128_t(int64_t value) { // NOLINT: fields are constructed by the template
    constructInt128Template(value, *this);
}

int128_t::int128_t(int32_t value) { // NOLINT: fields are constructed by the template
    constructInt128Template(value, *this);
}

int128_t::int128_t(int16_t value) { // NOLINT: fields are constructed by the template
    constructInt128Template(value, *this);
}

int128_t::int128_t(int8_t value) { // NOLINT: fields are constructed by the template
    constructInt128Template(value, *this);
}

int128_t::int128_t(uint64_t value) { // NOLINT: fields are constructed by the template
    constructInt128Template(value, *this);
}

int128_t::int128_t(uint32_t value) { // NOLINT: fields are constructed by the template
    constructInt128Template(value, *this);
}

int128_t::int128_t(uint16_t value) { // NOLINT: fields are constructed by the template
    constructInt128Template(value, *this);
}

int128_t::int128_t(uint8_t value) { // NOLINT: fields are constructed by the template
    constructInt128Template(value, *this);
}

int128_t::int128_t(double value) { // NOLINT: fields are constructed by the template
    constructInt128Template(value, *this);
}

int128_t::int128_t(float value) { // NOLINT: fields are constructed by the template
    constructInt128Template(value, *this);
}

//============================================================================================
bool operator==(const int128_t& lhs, const int128_t& rhs) {
    return Int128_t::equals(lhs, rhs);
}

bool operator!=(const int128_t& lhs, const int128_t& rhs) {
    return Int128_t::notEquals(lhs, rhs);
}

bool operator>(const int128_t& lhs, const int128_t& rhs) {
    return Int128_t::greaterThan(lhs, rhs);
}

bool operator>=(const int128_t& lhs, const int128_t& rhs) {
    return Int128_t::greaterThanOrEquals(lhs, rhs);
}

bool operator<(const int128_t& lhs, const int128_t& rhs) {
    return Int128_t::lessThan(lhs, rhs);
}

bool operator<=(const int128_t& lhs, const int128_t& rhs) {
    return Int128_t::lessThanOrEquals(lhs, rhs);
}

int128_t int128_t::operator-() const {
    return Int128_t::negate(*this);
}

// support for operations like (int32_t)x + (int128_t)y

int128_t operator+(const int128_t& lhs, const int128_t& rhs) {
    return Int128_t::Add(lhs, rhs);
}
int128_t operator-(const int128_t& lhs, const int128_t& rhs) {
    return Int128_t::Sub(lhs, rhs);
}
int128_t operator*(const int128_t& lhs, const int128_t& rhs) {
    return Int128_t::Mul(lhs, rhs);
}
int128_t operator/(const int128_t& lhs, const int128_t& rhs) {
    return Int128_t::Div(lhs, rhs);
}
int128_t operator%(const int128_t& lhs, const int128_t& rhs) {
    return Int128_t::Mod(lhs, rhs);
}

int128_t operator^(const int128_t& lhs, const int128_t& rhs) {
    return Int128_t::Xor(lhs, rhs);
}

int128_t operator&(const int128_t& lhs, const int128_t& rhs) {
    return Int128_t::BinaryAnd(lhs, rhs);
}

int128_t operator|(const int128_t& lhs, const int128_t& rhs) {
    return Int128_t::BinaryOr(lhs, rhs);
}

int128_t operator~(const int128_t& val) {
    return Int128_t::BinaryNot(val);
}

int128_t operator<<(const int128_t& lhs, int amount) {
    return Int128_t::LeftShift(lhs, amount);
}

int128_t operator>>(const int128_t& lhs, int amount) {
    return Int128_t::RightShift(lhs, amount);
}

// inplace arithmetic operators
int128_t& int128_t::operator+=(const int128_t& rhs) {
    if (!Int128_t::addInPlace(*this, rhs)) {
        throw common::OverflowException("INT128 is out of range: cannot add in place.");
    }
    return *this;
}

int128_t& int128_t::operator*=(const int128_t& rhs) {
    *this = Int128_t::Mul(*this, rhs);
    return *this;
}

int128_t& int128_t::operator|=(const int128_t& rhs) {
    *this = Int128_t::BinaryOr(*this, rhs);
    return *this;
}

int128_t& int128_t::operator&=(const int128_t& rhs) {
    *this = Int128_t::BinaryAnd(*this, rhs);
    return *this;
}

template<class T>
static T NarrowCast(const int128_t& input) {
    return static_cast<T>(input.low);
}

int128_t::operator int64_t() const {
    return NarrowCast<int64_t>(*this);
}

int128_t::operator int32_t() const {
    return NarrowCast<int32_t>(*this);
}

int128_t::operator int16_t() const {
    return NarrowCast<int16_t>(*this);
}

int128_t::operator int8_t() const {
    return NarrowCast<int8_t>(*this);
}

int128_t::operator uint64_t() const {
    return NarrowCast<uint64_t>(*this);
}

int128_t::operator uint32_t() const {
    return NarrowCast<uint32_t>(*this);
}

int128_t::operator uint16_t() const {
    return NarrowCast<uint16_t>(*this);
}

int128_t::operator uint8_t() const {
    return NarrowCast<uint8_t>(*this);
}

int128_t::operator double() const {
    double result = NAN;
    if (!Int128_t::tryCast(*this, result)) { // LCOV_EXCL_START
        throw common::OverflowException(
            std::format("Value {} is not within DOUBLE range", common::TypeUtils::toString(*this)));
    } // LCOV_EXCL_STOP
    return result;
}

int128_t::operator float() const {
    float result = NAN;
    if (!Int128_t::tryCast(*this, result)) { // LCOV_EXCL_START
        throw common::OverflowException(
            std::format("Value {} is not within FLOAT range", common::TypeUtils::toString(*this)));
    } // LCOV_EXCL_STOP
    return result;
}

int128_t::operator uint128_t() const {
    uint128_t result{};
    if (!Int128_t::tryCast(*this, result)) {
        throw common::OverflowException(std::format(
            "Cannot cast negative INT128 value {} to UINT128", common::TypeUtils::toString(*this)));
    }
    return result;
}

} // namespace riag::common

std::size_t std::hash<riag::common::int128_t>::operator()(
    const riag::common::int128_t& v) const noexcept {
    riag::common::hash_t hash = 0;
    riag::function::Hash::operation(v, hash);
    return hash;
}

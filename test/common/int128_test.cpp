#include <cstdint>
#include <random>
#include <string>
#include <vector>

#include "common/exception/overflow.h"
#include "common/exception/runtime.h"
#include "common/types/int128_t.h"
#include "gtest/gtest.h"

using namespace riag::common;

TEST(Int128Tests, LeftShiftTest) {
    {
        int128_t original_value{1ULL << 63, 0};
        int128_t expected_value{0, 1 << 2};
        EXPECT_EQ(expected_value, int128_t(1) << 66);
        EXPECT_EQ(expected_value, original_value << 3);
    }

    {
        int128_t original_value{-1};
        int128_t expected_value{-4};
        EXPECT_EQ(expected_value, original_value << 2);
    }

    {
        int128_t original_value{3, 55};

        static constexpr uint32_t shift_amount = 70;
        // the high bits will be discarded
        int128_t expected_value{0, 0b11 << (shift_amount - 64)};
        EXPECT_EQ(expected_value, int128_t(3) << shift_amount);
        EXPECT_EQ(expected_value, original_value << shift_amount);
    }

    {
        // ...11100
        int128_t original_value{-4};

        int128_t expected_value{0, -4};
        EXPECT_EQ(expected_value, original_value << 64);
    }
}

TEST(Int128Tests, RightShiftTest) {
    {
        int128_t original_value{0, 1 << 2};
        int128_t expected_value{1ULL << 63, 0};
        EXPECT_EQ(expected_value, original_value >> 3);
    }

    {
        int128_t original_value{0, -1};
        int128_t expected_value{-2};
        EXPECT_EQ(expected_value, original_value >> 63);
    }

    {
        int128_t original_value{3, 0b11 << 10};

        static constexpr uint32_t shift_amount = 70;
        int128_t expected_value{0b11 << 4}; // original low bits will be discarded
        EXPECT_EQ(expected_value, original_value >> shift_amount);
    }

    {
        // ...11100000
        int128_t original_value{0, -32};

        int128_t expected_value{-16};
        EXPECT_EQ(expected_value, original_value >> 65);
    }
}

// Arithmetic at the edges of the range. An operation has to succeed exactly when its result fits
// in 128 bits, INT128_MIN included, and report an overflow otherwise.
static const int128_t INT128_MIN_VALUE{0, INT64_MIN};
static const int128_t INT128_MAX_VALUE{UINT64_MAX, INT64_MAX};

TEST(Int128Tests, SubtractionResultingInMin) {
    EXPECT_EQ(INT128_MIN_VALUE, INT128_MIN_VALUE - int128_t(0));
    EXPECT_EQ(INT128_MIN_VALUE, (INT128_MIN_VALUE + int128_t(1)) - int128_t(1));
    EXPECT_EQ(INT128_MIN_VALUE, int128_t(-1) - INT128_MAX_VALUE);
    EXPECT_EQ(INT128_MIN_VALUE, int128_t(UINT64_MAX, INT64_MIN) - int128_t(UINT64_MAX, 0));
    EXPECT_EQ(INT128_MIN_VALUE, int128_t(0, INT64_MIN + 1) - int128_t(0, 1));
    EXPECT_THROW((void)(INT128_MIN_VALUE - int128_t(1)), OverflowException);
    EXPECT_THROW((void)(int128_t(-2) - INT128_MAX_VALUE), OverflowException);
    EXPECT_THROW((void)(int128_t(0) - INT128_MIN_VALUE), OverflowException);
    EXPECT_EQ(INT128_MAX_VALUE, int128_t(-1) - INT128_MIN_VALUE);
}

TEST(Int128Tests, MultiplicationInvolvingMin) {
    int128_t result{};
    EXPECT_TRUE(Int128_t::tryMultiply(INT128_MIN_VALUE, int128_t(1), result));
    EXPECT_EQ(INT128_MIN_VALUE, result);
    EXPECT_TRUE(Int128_t::tryMultiply(int128_t(1), INT128_MIN_VALUE, result));
    EXPECT_EQ(INT128_MIN_VALUE, result);
    EXPECT_TRUE(Int128_t::tryMultiply(INT128_MIN_VALUE, int128_t(0), result));
    EXPECT_EQ(int128_t(0), result);
    EXPECT_TRUE(Int128_t::tryMultiply(int128_t(0), INT128_MIN_VALUE, result));
    EXPECT_EQ(int128_t(0), result);
    // -2^64 * 2^63 = -2^127
    EXPECT_TRUE(Int128_t::tryMultiply(int128_t(0, -1), int128_t(1ULL << 63, 0), result));
    EXPECT_EQ(INT128_MIN_VALUE, result);
    EXPECT_TRUE(Int128_t::tryMultiply(int128_t(1ULL << 63, 0), int128_t(0, -1), result));
    EXPECT_EQ(INT128_MIN_VALUE, result);
    EXPECT_TRUE(Int128_t::tryMultiply(INT128_MAX_VALUE, int128_t(-1), result));
    EXPECT_EQ(INT128_MIN_VALUE + int128_t(1), result);
    // The products that do not fit report it through the return value, not an exception.
    EXPECT_FALSE(Int128_t::tryMultiply(INT128_MIN_VALUE, int128_t(2), result));
    EXPECT_FALSE(Int128_t::tryMultiply(int128_t(2), INT128_MIN_VALUE, result));
    EXPECT_FALSE(Int128_t::tryMultiply(INT128_MIN_VALUE, int128_t(-1), result));
    EXPECT_FALSE(Int128_t::tryMultiply(INT128_MIN_VALUE, INT128_MIN_VALUE, result));
    EXPECT_FALSE(Int128_t::tryMultiply(int128_t(0, -1), int128_t(1ULL << 63, 1), result));
    EXPECT_FALSE(Int128_t::tryMultiply(INT128_MAX_VALUE, int128_t(2), result));
    EXPECT_THROW((void)(INT128_MIN_VALUE * int128_t(2)), OverflowException);
    EXPECT_THROW((void)(INT128_MIN_VALUE * int128_t(-1)), OverflowException);
    EXPECT_EQ(INT128_MIN_VALUE, INT128_MIN_VALUE * int128_t(1));
}

TEST(Int128Tests, DivisionAndModuloInvolvingMin) {
    EXPECT_EQ(INT128_MIN_VALUE, INT128_MIN_VALUE / int128_t(1));
    EXPECT_EQ(int128_t(0), INT128_MIN_VALUE % int128_t(1));
    EXPECT_EQ(int128_t(0, -0x4000000000000000LL), INT128_MIN_VALUE / int128_t(2)); // -2^126
    EXPECT_EQ(int128_t(0), INT128_MIN_VALUE % int128_t(2));
    EXPECT_EQ(int128_t(-1), INT128_MIN_VALUE / INT128_MAX_VALUE);
    EXPECT_EQ(int128_t(-1), INT128_MIN_VALUE % INT128_MAX_VALUE);
    EXPECT_EQ(int128_t(1), INT128_MIN_VALUE / INT128_MIN_VALUE);
    EXPECT_EQ(int128_t(0), INT128_MIN_VALUE % INT128_MIN_VALUE);
    EXPECT_EQ(int128_t(0), int128_t(5) / INT128_MIN_VALUE);
    EXPECT_EQ(int128_t(5), int128_t(5) % INT128_MIN_VALUE);
    EXPECT_EQ(int128_t(-5), int128_t(-5) % INT128_MIN_VALUE);
    // The quotient 2^127 does not fit; the remainder 0 does.
    EXPECT_THROW((void)(INT128_MIN_VALUE / int128_t(-1)), OverflowException);
    EXPECT_EQ(int128_t(0), INT128_MIN_VALUE % int128_t(-1));
    EXPECT_EQ(INT128_MAX_VALUE, INT128_MAX_VALUE / int128_t(1));
    EXPECT_EQ(INT128_MIN_VALUE + int128_t(1), INT128_MAX_VALUE / int128_t(-1));
    EXPECT_THROW((void)(INT128_MIN_VALUE / int128_t(0)), RuntimeException);
    EXPECT_THROW((void)(INT128_MIN_VALUE % int128_t(0)), RuntimeException);
}

TEST(Int128Tests, NegationAtTheEdges) {
    EXPECT_THROW((void)(-INT128_MIN_VALUE), OverflowException);
    EXPECT_EQ(INT128_MAX_VALUE, -(INT128_MIN_VALUE + int128_t(1)));
    EXPECT_EQ(INT128_MIN_VALUE + int128_t(1), -INT128_MAX_VALUE);
    // Values whose high part is INT64_MIN but which are not INT128_MIN.
    EXPECT_EQ(int128_t(UINT64_MAX, INT64_MAX), -int128_t(1, INT64_MIN));
    EXPECT_EQ(int128_t(1, INT64_MIN), -int128_t(UINT64_MAX, INT64_MAX));
    EXPECT_EQ(int128_t(0, 1), -int128_t(0, -1));
    EXPECT_EQ(int128_t(0, -1), -int128_t(0, 1));
    EXPECT_EQ(int128_t(0), -int128_t(0));
}

#ifdef __SIZEOF_INT128__
namespace {
using native_t = __int128;

native_t toNative(int128_t value) {
    return (native_t(value.high) << 64) | native_t(value.low);
}

int128_t fromNative(native_t value) {
    return int128_t{uint64_t(value), int64_t(value >> 64)};
}

std::vector<native_t> interestingValues() {
    const native_t maxValue = toNative(INT128_MAX_VALUE);
    const native_t minValue = toNative(INT128_MIN_VALUE);
    std::vector<native_t> bases;
    for (int shift : {0, 1, 2, 31, 32, 33, 62, 63, 64, 65, 66, 95, 96, 126}) {
        bases.push_back(native_t(1) << shift);
    }
    bases.push_back(maxValue);
    bases.push_back(minValue);
    bases.push_back(10);
    bases.push_back(1000000007);
    bases.push_back(native_t(UINT64_MAX));
    bases.push_back((native_t(0x5555555555555555ULL) << 64) | native_t(0x5555555555555555ULL));
    std::vector<native_t> values;
    for (auto base : bases) {
        for (int delta = -3; delta <= 3; delta++) {
            // Wrapping arithmetic keeps the values near +-2^127 well defined.
            auto candidate = native_t(__uint128_t(base) + __uint128_t(native_t(delta)));
            values.push_back(candidate);
            values.push_back(native_t(__uint128_t(0) - __uint128_t(candidate)));
        }
    }
    return values;
}

void checkBinaryOperations(native_t lhsNative, native_t rhsNative) {
    auto lhs = fromNative(lhsNative);
    auto rhs = fromNative(rhsNative);
    SCOPED_TRACE(Int128_t::toString(lhs) + " , " + Int128_t::toString(rhs));
    native_t expected = 0;

    if (__builtin_add_overflow(lhsNative, rhsNative, &expected)) {
        EXPECT_THROW((void)(lhs + rhs), OverflowException);
    } else {
        EXPECT_EQ(fromNative(expected), lhs + rhs);
    }

    if (__builtin_sub_overflow(lhsNative, rhsNative, &expected)) {
        EXPECT_THROW((void)(lhs - rhs), OverflowException);
    } else {
        EXPECT_EQ(fromNative(expected), lhs - rhs);
    }

    int128_t product{};
    bool tryResult = false;
    EXPECT_NO_THROW(tryResult = Int128_t::tryMultiply(lhs, rhs, product));
    if (__builtin_mul_overflow(lhsNative, rhsNative, &expected)) {
        EXPECT_FALSE(tryResult);
        EXPECT_THROW((void)(lhs * rhs), OverflowException);
    } else {
        EXPECT_TRUE(tryResult);
        EXPECT_EQ(fromNative(expected), product);
        EXPECT_EQ(fromNative(expected), lhs * rhs);
    }

    if (rhsNative == 0) {
        EXPECT_THROW((void)(lhs / rhs), RuntimeException);
        EXPECT_THROW((void)(lhs % rhs), RuntimeException);
    } else if (lhsNative == toNative(INT128_MIN_VALUE) && rhsNative == -1) {
        EXPECT_THROW((void)(lhs / rhs), OverflowException);
        EXPECT_EQ(int128_t(0), lhs % rhs);
    } else {
        EXPECT_EQ(fromNative(lhsNative / rhsNative), lhs / rhs);
        EXPECT_EQ(fromNative(lhsNative % rhsNative), lhs % rhs);
    }
}
} // namespace

// Compares the operations with the compiler's native 128-bit integer on the values around every
// boundary of the representation (powers of two, the limits and their neighbours, both signs).
TEST(Int128Tests, ArithmeticAgreesWithNativeAtBoundaries) {
    auto values = interestingValues();
    for (auto lhs : values) {
        for (auto rhs : values) {
            checkBinaryOperations(lhs, rhs);
        }
    }
    for (auto value : values) {
        if (value == toNative(INT128_MIN_VALUE)) {
            EXPECT_THROW((void)(-fromNative(value)), OverflowException);
        } else {
            EXPECT_EQ(fromNative(-value), -fromNative(value));
        }
    }
}

TEST(Int128Tests, ArithmeticAgreesWithNativeOnRandomValues) {
    std::mt19937_64 random(20261112);
    auto next = [&]() {
        // Mix full-width values with ones of random bit length so that products and quotients
        // land on both sides of the limits.
        auto bits = random() % 129;
        __uint128_t value = (__uint128_t(random()) << 64) | random();
        if (bits < 128) {
            value &= (__uint128_t(1) << bits) - 1;
        }
        if (random() % 2) {
            value = __uint128_t(0) - value;
        }
        return native_t(value);
    };
    for (int i = 0; i < 200000; i++) {
        checkBinaryOperations(next(), next());
    }
}
#endif

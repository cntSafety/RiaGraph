#pragma once

#include <cmath>
#include <type_traits>

#include "common/vector/value_vector.h"
#include "function/comparison/comparison_operation.h"

namespace riag {
namespace function {

struct Equals {
    static constexpr auto comparisonOperation = ComparisonOperation::EQUAL;

    template<class A, class B>
    static inline void operation(const A& left, const B& right, uint8_t& result,
        common::ValueVector* /*leftVector*/, common::ValueVector* /*rightVector*/) {
        result = left == right;
    }

    template<class T>
    static bool operation(const T& left, const T& right) {
        uint8_t result = 0;
        operation<T>(left, right, result, nullptr, nullptr);
        return result;
    }
};

struct NotEquals {
    static constexpr auto comparisonOperation = ComparisonOperation::NOT_EQUAL;

    template<class A, class B>
    static inline void operation(const A& left, const B& right, uint8_t& result,
        common::ValueVector* leftVector, common::ValueVector* rightVector) {
        Equals::operation(left, right, result, leftVector, rightVector);
        result = !result;
    }

    template<class T>
    static bool operation(const T& left, const T& right) {
        uint8_t result = 0;
        operation<T>(left, right, result, nullptr, nullptr);
        return result;
    }
};

struct GreaterThan {
    static constexpr auto comparisonOperation = ComparisonOperation::GREATER_THAN;

    template<class A, class B>
    static inline void operation(const A& left, const B& right, uint8_t& result,
        common::ValueVector* /*leftVector*/, common::ValueVector* /*rightVector*/) {
        result = left > right;
    }

    template<class T>
    static bool operation(const T& left, const T& right) {
        uint8_t result = 0;
        operation<T>(left, right, result, nullptr, nullptr);
        return result;
    }
};

struct GreaterThanEquals {
    static constexpr auto comparisonOperation = ComparisonOperation::GREATER_THAN_EQUAL;

    template<class A, class B>
    static inline void operation(const A& left, const B& right, uint8_t& result,
        common::ValueVector* leftVector, common::ValueVector* rightVector) {
        uint8_t isGreater = 0;
        uint8_t isEqual = 0;
        GreaterThan::operation(left, right, isGreater, leftVector, rightVector);
        Equals::operation(left, right, isEqual, leftVector, rightVector);
        result = isGreater || isEqual;
    }

    template<class T>
    static bool operation(const T& left, const T& right) {
        uint8_t result = 0;
        operation<T>(left, right, result, nullptr, nullptr);
        return result;
    }
};

// For floating point, a < b is b > a and a <= b is b >= a: the negations of >= and >
// used for every other type are true whenever an operand is NaN, so `x < 0.0` matched NaN
// rows and NaN compared less than every value and every value less than NaN. Swapped, NaN
// compares false, as it already did for =, > and >=. Other types keep the negation: for
// some, such as INTERVAL, > orders values (1 month as 30 days) that = tells apart, so b >= a
// is not the same as !(a > b) there.
template<class A, class B>
inline constexpr bool isFloatingPointComparison =
    std::is_floating_point_v<A> && std::is_floating_point_v<B>;

struct LessThan {
    static constexpr auto comparisonOperation = ComparisonOperation::LESS_THAN;

    template<class A, class B>
    static inline void operation(const A& left, const B& right, uint8_t& result,
        common::ValueVector* leftVector, common::ValueVector* rightVector) {
        if constexpr (isFloatingPointComparison<A, B>) {
            result = right > left;
        } else {
            GreaterThanEquals::operation(left, right, result, leftVector, rightVector);
            result = !result;
        }
    }

    template<class T>
    static bool operation(const T& left, const T& right) {
        uint8_t result = 0;
        operation<T>(left, right, result, nullptr, nullptr);
        return result;
    }
};

struct LessThanEquals {
    static constexpr auto comparisonOperation = ComparisonOperation::LESS_THAN_EQUAL;

    template<class A, class B>
    static inline void operation(const A& left, const B& right, uint8_t& result,
        common::ValueVector* leftVector, common::ValueVector* rightVector) {
        if constexpr (isFloatingPointComparison<A, B>) {
            result = right > left || right == left;
        } else {
            GreaterThan::operation(left, right, result, leftVector, rightVector);
            result = !result;
        }
    }

    template<class T>
    static bool operation(const T& left, const T& right) {
        uint8_t result = 0;
        operation<T>(left, right, result, nullptr, nullptr);
        return result;
    }
};

// ORDER BY's order, which anything that picks an extreme or a boundary has to agree with
// (min/max, top-k): NaN sorts above every other floating-point value and equals itself.
// The operators above are false for any NaN operand, so an extreme chosen by them alone
// depends on where in the input a NaN falls. Other types order exactly as the operators do.
// These carry no comparisonOperation, so selectComparison never hands them to the SIMD
// kernels, which implement the IEEE operators.
struct TotalOrderGreaterThan {
    template<class A, class B>
    static inline void operation(const A& left, const B& right, uint8_t& result,
        common::ValueVector* leftVector, common::ValueVector* rightVector) {
        if constexpr (isFloatingPointComparison<A, B>) {
            if (std::isnan(left) || std::isnan(right)) {
                result = std::isnan(left) && !std::isnan(right);
                return;
            }
        }
        GreaterThan::operation(left, right, result, leftVector, rightVector);
    }

    template<class T>
    static bool operation(const T& left, const T& right) {
        uint8_t result = 0;
        operation<T>(left, right, result, nullptr, nullptr);
        return result;
    }
};

struct TotalOrderLessThan {
    template<class A, class B>
    static inline void operation(const A& left, const B& right, uint8_t& result,
        common::ValueVector* leftVector, common::ValueVector* rightVector) {
        if constexpr (isFloatingPointComparison<A, B>) {
            TotalOrderGreaterThan::operation(right, left, result, rightVector, leftVector);
        } else {
            LessThan::operation(left, right, result, leftVector, rightVector);
        }
    }

    template<class T>
    static bool operation(const T& left, const T& right) {
        uint8_t result = 0;
        operation<T>(left, right, result, nullptr, nullptr);
        return result;
    }
};

struct TotalOrderEquals {
    template<class A, class B>
    static inline void operation(const A& left, const B& right, uint8_t& result,
        common::ValueVector* leftVector, common::ValueVector* rightVector) {
        if constexpr (isFloatingPointComparison<A, B>) {
            if (std::isnan(left) || std::isnan(right)) {
                result = std::isnan(left) && std::isnan(right);
                return;
            }
        }
        Equals::operation(left, right, result, leftVector, rightVector);
    }

    template<class T>
    static bool operation(const T& left, const T& right) {
        uint8_t result = 0;
        operation<T>(left, right, result, nullptr, nullptr);
        return result;
    }
};

// specialization for equal and greater than.
template<>
void Equals::operation(const common::list_entry_t& left, const common::list_entry_t& right,
    uint8_t& result, common::ValueVector* leftVector, common::ValueVector* rightVector);
template<>
void Equals::operation(const common::struct_entry_t& left, const common::struct_entry_t& right,
    uint8_t& result, common::ValueVector* leftVector, common::ValueVector* rightVector);
template<>
void GreaterThan::operation(const common::list_entry_t& left, const common::list_entry_t& right,
    uint8_t& result, common::ValueVector* leftVector, common::ValueVector* rightVector);
template<>
void GreaterThan::operation(const common::struct_entry_t& left, const common::struct_entry_t& right,
    uint8_t& result, common::ValueVector* leftVector, common::ValueVector* rightVector);

} // namespace function
} // namespace riag

#include "binder/expression/expression.h"
#include "common/exception/binder.h"
#include "expression_evaluator/expression_evaluator_utils.h"
#include "function/string/functions/base_regexp_function.h"
#include "function/string/vector_string_functions.h"
#include "re2.h"
#include <format>

namespace riag {
namespace function {

using namespace common;

struct RegexFullMatchBindData : public FunctionBindData {
    regex::RE2 pattern;

    explicit RegexFullMatchBindData(common::logical_type_vec_t paramTypes, std::string patternInStr)
        : FunctionBindData{std::move(paramTypes), common::LogicalType::BOOL()},
          pattern{patternInStr} {}

    std::unique_ptr<FunctionBindData> copy() const override {
        return std::make_unique<RegexFullMatchBindData>(copyVector(paramTypes), pattern.pattern());
    }
};

struct RegexpFullMatch {
    static void operation(string_t& left, string_t& right, uint8_t& result,
        common::ValueVector& /*leftValueVector*/, common::ValueVector& /*rightValueVector*/,
        common::ValueVector& /*resultValueVector*/, void* dataPtr) {
        result = RE2::FullMatch(BaseRegexpOperation::toStringPiece(left),
            RegexpBindData::getRegex(dataPtr, right));
    }
};

struct RegexpFullMatchStaticPattern : BaseRegexpOperation {
    static void operation(string_t& left, string_t& /*right*/, uint8_t& result,
        common::ValueVector& /*leftValueVector*/, common::ValueVector& /*rightValueVector*/,
        common::ValueVector& /*resultValueVector*/, void* dataPtr) {
        auto regexFullMatchBindData = reinterpret_cast<RegexFullMatchBindData*>(dataPtr);
        result = RE2::FullMatch(left.getAsString(), regexFullMatchBindData->pattern);
    }
};

// =~ matches STRING against STRING only, as in Cypher: any other operand is a type error, not
// cast. A NULL literal or a parameter of unknown type has type ANY and is bound as a STRING.
static void validateOperandType(const binder::Expression& operand) {
    const auto& type = operand.getDataType();
    if (type.getLogicalTypeID() == LogicalTypeID::STRING ||
        type.getLogicalTypeID() == LogicalTypeID::ANY) {
        return;
    }
    throw BinderException(
        std::format("Type mismatch: the operands of =~ must be STRING, but {} has data type {}.",
            operand.toString(), type.toString()));
}

// The parameter types of the bind data are the types the binder casts the arguments to, and the
// execution functions read both arguments as STRING: they must be the declared types, never the
// types of the arguments.
static std::unique_ptr<FunctionBindData> regexFullMatchBindFunc(const ScalarBindFuncInput& input) {
    for (auto& argument : input.arguments) {
        validateOperandType(*argument);
    }
    auto paramTypes = logical_type_vec_t{};
    paramTypes.push_back(LogicalType::STRING());
    paramTypes.push_back(LogicalType::STRING());
    const auto& pattern = input.arguments[1];
    if (pattern->expressionType == ExpressionType::LITERAL &&
        pattern->getDataType().getLogicalTypeID() == LogicalTypeID::STRING) {
        auto value =
            evaluator::ExpressionEvaluatorUtils::evaluateConstantExpression(pattern, input.context);
        if (!value.isNull()) {
            input.definition->ptrCast<ScalarFunction>()->execFunc =
                ScalarFunction::BinaryExecWithBindData<string_t, string_t, uint8_t,
                    RegexpFullMatchStaticPattern>;
            input.definition->ptrCast<ScalarFunction>()->selectFunc =
                ScalarFunction::BinarySelectWithBindData<string_t, string_t,
                    RegexpFullMatchStaticPattern>;
            return std::make_unique<RegexFullMatchBindData>(std::move(paramTypes),
                BaseRegexpOperation::parseCypherPattern(value.getValue<std::string>()));
        }
    }
    return std::make_unique<RegexpBindData>(std::move(paramTypes), LogicalType::BOOL());
}

function_set RegexpFullMatchFunction::getFunctionSet() {
    function_set functionSet;
    auto scalarFunc = make_unique<ScalarFunction>(name,
        std::vector<LogicalTypeID>{LogicalTypeID::STRING, LogicalTypeID::STRING},
        LogicalTypeID::BOOL,
        ScalarFunction::BinaryExecWithBindData<string_t, string_t, uint8_t, RegexpFullMatch>,
        ScalarFunction::BinarySelectWithBindData<string_t, string_t, RegexpFullMatch>);
    scalarFunc->bindFunc = regexFullMatchBindFunc;
    functionSet.emplace_back(std::move(scalarFunc));
    return functionSet;
}

} // namespace function
} // namespace riag

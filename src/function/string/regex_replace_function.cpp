#include "binder/expression/expression_util.h"
#include "common/exception/binder.h"
#include "expression_evaluator/expression_evaluator_utils.h"
#include "function/string/functions/base_regexp_function.h"
#include "function/string/vector_string_functions.h"
#include "re2.h"

namespace riag {
namespace function {

using namespace common;

using re2_replace_func_t =
    std::function<void(std::string* str, const RE2& re, const regex::StringPiece& rewrite)>;

// A pattern that is not a literal is compiled as given, without the Cypher escape handling a
// literal pattern gets.
struct RegexReplaceBindData : public RegexpBindData {
    re2_replace_func_t replaceFunc;

    RegexReplaceBindData(common::logical_type_vec_t paramTypes, re2_replace_func_t replaceFunc)
        : RegexpBindData{std::move(paramTypes), common::LogicalType::STRING(),
              false /* parseCypherEscapes */},
          replaceFunc{std::move(replaceFunc)} {}

    std::unique_ptr<FunctionBindData> copy() const override {
        return std::make_unique<RegexReplaceBindData>(copyVector(paramTypes), replaceFunc);
    }
};

struct RegexpReplace {
    static void operation(string_t& value, string_t& pattern, string_t& replacement,
        string_t& result, common::ValueVector& resultValueVector, void* dataPtr) {
        auto bindData = reinterpret_cast<RegexReplaceBindData*>(dataPtr);
        std::string resultStr = value.getAsString();
        bindData->replaceFunc(&resultStr, RegexpBindData::getRegex(dataPtr, pattern),
            replacement.getAsString());
        BaseRegexpOperation::copyToRiaGString(resultStr, result, resultValueVector);
    }
};

struct RegexReplaceBindDataStaticPattern : public RegexReplaceBindData {
    regex::RE2 pattern;

    RegexReplaceBindDataStaticPattern(common::logical_type_vec_t paramTypes,
        re2_replace_func_t replaceFunc, std::string patternInStr)
        : RegexReplaceBindData{std::move(paramTypes), std::move(replaceFunc)},
          pattern{patternInStr} {}

    std::unique_ptr<FunctionBindData> copy() const override {
        return std::make_unique<RegexReplaceBindDataStaticPattern>(copyVector(paramTypes),
            replaceFunc, pattern.pattern());
    }
};

struct RegexpReplaceStaticPattern {
    static void operation(string_t& value, string_t& /*pattern*/, string_t& replacement,
        string_t& result, common::ValueVector& resultValueVector, void* dataPtr) {
        auto bindData = reinterpret_cast<RegexReplaceBindDataStaticPattern*>(dataPtr);
        auto resultStr = value.getAsString();
        bindData->replaceFunc(&resultStr, bindData->pattern, replacement.getAsString());
        BaseRegexpOperation::copyToRiaGString(resultStr, result, resultValueVector);
    }
};

static re2_replace_func_t bindReplaceFunc(const binder::expression_vector& expr) {
    re2_replace_func_t result;
    switch (expr.size()) {
    case 3: {
        result = RE2::Replace;
    } break;
    case 4: {
        result = RE2::GlobalReplace;
    } break;
    default:
        UNREACHABLE_CODE;
    }
    return result;
}

template<typename OP>
scalar_func_exec_t getExecFunc(const binder::expression_vector& expr) {
    scalar_func_exec_t execFunc;
    switch (expr.size()) {
    case 3: {
        execFunc =
            ScalarFunction::TernaryRegexExecFunction<string_t, string_t, string_t, string_t, OP>;
    } break;
    case 4: {
        auto option = expr[3];
        binder::ExpressionUtil::validateExpressionType(*option, ExpressionType::LITERAL);
        binder::ExpressionUtil::validateDataType(*option, LogicalType::STRING());
        auto optionVal = binder::ExpressionUtil::getLiteralValue<std::string>(*option);
        if (optionVal != RegexpReplaceFunction::GLOBAL_REPLACE_OPTION) {
            throw common::BinderException{
                "regex_replace can only support global replace option: g."};
        }
        execFunc =
            ScalarFunction::TernaryRegexExecFunction<string_t, string_t, string_t, string_t, OP>;
    } break;
    default:
        UNREACHABLE_CODE;
    }
    return execFunc;
}

// The parameter types of the bind data are the types the binder casts the arguments to, and the
// execution functions read every argument as STRING: they must be the declared types, never the
// types of the arguments, so a non-STRING argument is cast as for the other regexp functions.
std::unique_ptr<FunctionBindData> bindFunc(ScalarBindFuncInput input) {
    auto definition = input.definition->ptrCast<ScalarFunction>();
    re2_replace_func_t replaceFunc = bindReplaceFunc(input.arguments);
    auto paramTypes = logical_type_vec_t{};
    for (auto typeID : definition->parameterTypeIDs) {
        paramTypes.push_back(LogicalType(typeID));
    }
    const auto& pattern = input.arguments[1];
    if (pattern->expressionType == ExpressionType::LITERAL &&
        pattern->getDataType().getLogicalTypeID() == LogicalTypeID::STRING) {
        auto value =
            evaluator::ExpressionEvaluatorUtils::evaluateConstantExpression(pattern, input.context);
        if (!value.isNull()) {
            definition->execFunc = getExecFunc<RegexpReplaceStaticPattern>(input.arguments);
            return std::make_unique<RegexReplaceBindDataStaticPattern>(std::move(paramTypes),
                std::move(replaceFunc),
                BaseRegexpOperation::parseCypherPattern(value.getValue<std::string>()));
        }
    }
    definition->execFunc = getExecFunc<RegexpReplace>(input.arguments);
    return std::make_unique<RegexReplaceBindData>(std::move(paramTypes), std::move(replaceFunc));
}

function_set RegexpReplaceFunction::getFunctionSet() {
    function_set functionSet;
    std::unique_ptr<ScalarFunction> func;
    func = std::make_unique<ScalarFunction>(name,
        std::vector<LogicalTypeID>{LogicalTypeID::STRING, LogicalTypeID::STRING,
            LogicalTypeID::STRING, LogicalTypeID::STRING},
        LogicalTypeID::STRING);
    func->bindFunc = bindFunc;
    functionSet.emplace_back(std::move(func));
    func = std::make_unique<ScalarFunction>(name,
        std::vector<LogicalTypeID>{LogicalTypeID::STRING, LogicalTypeID::STRING,
            LogicalTypeID::STRING},
        LogicalTypeID::STRING);
    func->bindFunc = bindFunc;
    functionSet.emplace_back(std::move(func));
    return functionSet;
}

} // namespace function
} // namespace riag

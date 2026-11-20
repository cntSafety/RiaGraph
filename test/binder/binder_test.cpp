#include "binder/binder.h"
#include "binder/expression/literal_expression.h"
#include "binder/visitor/confidential_statement_analyzer.h"
#include "function/cast/cast_function_bind_data.h"
#include "function/cast/vector_cast_functions.h"
#include "graph_test/private_graph_test.h"
#include "parser/parser.h"

namespace riag {
namespace testing {

TEST_F(EmptyDBTest, ConfidentialStatementVisitor) {
    createDBAndConn();
    conn->query("BEGIN TRANSACTION");
    database->addExtensionOption("binder_test_conf", common::LogicalTypeID::UINT64,
        common::Value{5}, true /* isConfidential */);
    database->addExtensionOption("binder_test_not_conf", common::LogicalTypeID::UINT64,
        common::Value{2}, false /* isConfidential */);
    binder::Binder binder{conn->getClientContext()};

    binder::ConfidentialStatementAnalyzer analyzerConfidential{};
    auto parsed = parser::Parser::parseQuery("CALL binder_test_conf = 3");
    auto boundStatement = binder.bind(*parsed[0]);
    analyzerConfidential.visit(*boundStatement);
    ASSERT_TRUE(analyzerConfidential.isConfidential());

    binder::ConfidentialStatementAnalyzer analyzerNotConfidential{};
    parsed = parser::Parser::parseQuery("CALL binder_test_not_conf = 444");
    boundStatement = binder.bind(*parsed[0]);
    analyzerConfidential.visit(*boundStatement);
    ASSERT_FALSE(analyzerConfidential.isConfidential());

    parsed = parser::Parser::parseQuery("CALL timeout = 4");
    boundStatement = binder.bind(*parsed[0]);
    analyzerNotConfidential.visit(*boundStatement);
    ASSERT_FALSE(analyzerNotConfidential.isConfidential());
}

TEST_F(EmptyDBTest, LargeConstantListIsNotFolded) {
    createDBAndConn();
    conn->query("BEGIN TRANSACTION");
    auto bindReturnType = [&](const std::string& expression) {
        binder::Binder binder{conn->getClientContext()};
        auto parsed = parser::Parser::parseQuery("RETURN " + expression);
        return binder.bind(*parsed[0])->getStatementResult()->getColumns()[0]->expressionType;
    };
    const auto literal = common::ExpressionType::LITERAL;
    const auto function = common::ExpressionType::FUNCTION;
    EXPECT_EQ(bindReturnType("range(1, 8192)"), literal);
    EXPECT_EQ(bindReturnType("range(1, 8193)"), function);
    EXPECT_EQ(bindReturnType("[range(1, 9000)]"), function);
    EXPECT_EQ(bindReturnType("{a: range(1, 9000)}"), function);
    // Built from folded parts, or written out in the query: no larger than the expression.
    EXPECT_EQ(bindReturnType("[range(1, 5000), range(1, 5000)]"), literal);
    std::string writtenOutList = "[0";
    for (auto i = 1; i < 10000; i++) {
        writtenOutList += ", " + std::to_string(i);
    }
    writtenOutList += "]";
    EXPECT_EQ(bindReturnType(writtenOutList), literal);
    EXPECT_EQ(bindReturnType("CASE WHEN true THEN " + writtenOutList + " ELSE [1] END"), literal);
}

// The string executors read the cast options through a downcast of the bind data to
// CastFunctionBindData, so every function that casts from a STRING has to bind one. The functions
// below are called directly (TIMESTAMP(s), BOOL(s), ...) and have no bindFunc of their own.
TEST_F(EmptyDBTest, CastFromStringFunctionsBindCastOptions) {
    createDBAndConn();
    binder::expression_vector arguments{
        std::make_shared<binder::LiteralExpression>(common::Value{std::string{"1"}}, "arg")};
    function::function_set functions;
    auto addFunctions = [&](function::function_set set) {
        for (auto& func : set) {
            functions.push_back(std::move(func));
        }
    };
    addFunctions(function::CastToDateFunction::getFunctionSet());
    addFunctions(function::CastToTimestampFunction::getFunctionSet());
    addFunctions(function::CastToIntervalFunction::getFunctionSet());
    addFunctions(function::CastToBlobFunction::getFunctionSet());
    addFunctions(function::CastToUUIDFunction::getFunctionSet());
    addFunctions(function::CastToBoolFunction::getFunctionSet());
    addFunctions(function::CastToDoubleFunction::getFunctionSet());
    addFunctions(function::CastToInt64Function::getFunctionSet());
    auto numStringFunctions = 0u;
    for (auto& func : functions) {
        if (func->parameterTypeIDs[0] != common::LogicalTypeID::STRING) {
            continue;
        }
        numStringFunctions++;
        auto& scalarFunc = *func->ptrCast<function::ScalarFunction>();
        ASSERT_NE(scalarFunc.bindFunc, nullptr) << func->name;
        function::ScalarBindFuncInput input{arguments, func.get(), conn->getClientContext(), {}};
        auto bindData = scalarFunc.bindFunc(input);
        EXPECT_NE(dynamic_cast<function::CastFunctionBindData*>(bindData.get()), nullptr)
            << func->name;
    }
    EXPECT_EQ(numStringFunctions, 8u);
}

} // namespace testing
} // namespace riag

#pragma once

// ANTLR4 generates code with unused parameters.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
// The ASSERT keyword gives the generated contexts an ASSERT() accessor, which the ASSERT(condition)
// macro of common/assert.h would expand. Push/pop to keep the fix local.
#pragma push_macro("ASSERT")
#undef ASSERT
#include "cypher_parser.h"
#pragma pop_macro("ASSERT")
#pragma GCC diagnostic pop

namespace riag {
namespace parser {

class RiaGCypherParser : public CypherParser {

public:
    explicit RiaGCypherParser(antlr4::TokenStream* input) : CypherParser(input) {}

    void notifyQueryNotConcludeWithReturn(antlr4::Token* startToken) override;

    void notifyNodePatternWithoutParentheses(std::string nodeName,
        antlr4::Token* startToken) override;

    void notifyInvalidNotEqualOperator(antlr4::Token* startToken) override;

    void notifyEmptyToken(antlr4::Token* startToken) override;

    void notifyReturnNotAtEnd(antlr4::Token* startToken) override;

    void notifyNonBinaryComparison(antlr4::Token* startToken) override;
};

} // namespace parser
} // namespace riag

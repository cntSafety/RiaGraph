#include "parser/parser.h"

// ANTLR4 generates code with unused parameters.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include "cypher_lexer.h"
#pragma GCC diagnostic pop

#include "common/exception/parser.h"
#include "common/string_utils.h"
#include "parser/antlr_parser/parser_error_listener.h"
#include "parser/antlr_parser/parser_error_strategy.h"
#include "parser/antlr_parser/riag_cypher_parser.h"
#include "parser/transformer.h"

using namespace antlr4;

namespace riag {
namespace parser {

std::vector<std::shared_ptr<Statement>> Parser::parseQuery(std::string_view query,
    std::vector<extension::TransformerExtension*> transformerExtensions) {
    auto queryStr = std::string(query);
    queryStr = common::StringUtils::ltrim(queryStr);
    queryStr = common::StringUtils::ltrimNewlines(queryStr);
    // LCOV_EXCL_START
    // We should have enforced this in connection, but I also realize empty query will cause
    // antlr to hang. So enforce a duplicate check here.
    if (queryStr.empty()) {
        throw common::ParserException(
            "Cannot parse empty query. This should be handled in connection.");
    }
    // LCOV_EXCL_STOP

    auto inputStream = ANTLRInputStream(queryStr);
    // ANTLR drops a leading UTF-8 byte-order mark when it loads the input, so a query that is only
    // a BOM (possibly after whitespace) is empty here although queryStr is not. Check the stream
    // the lexer will actually read.
    if (inputStream.size() == 0) {
        throw common::ParserException("Cannot parse empty query.");
    }
    auto parserErrorListener = ParserErrorListener();

    auto cypherLexer = CypherLexer(&inputStream);
    cypherLexer.removeErrorListeners();
    cypherLexer.addErrorListener(&parserErrorListener);
    auto tokens = CommonTokenStream(&cypherLexer);
    tokens.fill();

    auto riagCypherParser = RiaGCypherParser(&tokens);
    riagCypherParser.removeErrorListeners();
    riagCypherParser.addErrorListener(&parserErrorListener);
    riagCypherParser.setErrorHandler(std::make_shared<ParserErrorStrategy>());

    Transformer transformer(*riagCypherParser.iC_Statements(), std::move(transformerExtensions));
    return transformer.transform();
}

} // namespace parser
} // namespace riag

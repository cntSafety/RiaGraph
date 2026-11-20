#include "common/exception/parser.h"
#include "gtest/gtest.h"
#include "parser/parser.h"

using namespace riag::parser;
using namespace riag::common;

// The ANTLR input stream drops a leading UTF-8 byte-order mark, so a query that is nothing but a
// BOM used to reach the lexer as an empty input, whose EOF token has stop = SIZE_MAX, and the
// parser's error formatter looped over that range until it ran out of memory.
TEST(ParserInputTest, QueryThatIsOnlyAByteOrderMarkIsEmpty) {
    for (const char* query : {"\xEF\xBB\xBF", "\xEF\xBB\xBF \n\t", " \xEF\xBB\xBF"}) {
        EXPECT_THROW(Parser::parseQuery(query), ParserException) << query;
    }
}

TEST(ParserInputTest, LeadingByteOrderMarkIsIgnored) {
    auto statements = Parser::parseQuery("\xEF\xBB\xBF RETURN 1");
    EXPECT_EQ(statements.size(), 1);
    statements = Parser::parseQuery("\xEF\xBB\xBFRETURN 1");
    EXPECT_EQ(statements.size(), 1);
}

TEST(ParserInputTest, SyntaxErrorAtEndOfInputStillUnderlinesNothing) {
    try {
        Parser::parseQuery("MATCH (a) RETURN");
        FAIL();
    } catch (const ParserException& e) {
        EXPECT_NE(std::string(e.what()).find("<EOF>"), std::string::npos);
    }
}

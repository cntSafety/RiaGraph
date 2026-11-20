#include "common/exception/parser.h"
#include "gtest/gtest.h"
#include "parser/copy.h"
#include "parser/ddl/alter.h"
#include "parser/ddl/create_table.h"
#include "parser/namespace_statement.h"
#include "parser/parser.h"
#include "parser/query/reading_clause/match_clause.h"
#include "parser/query/regular_query.h"
#include "parser/query/updating_clause/insert_clause.h"
#include "parser/query/updating_clause/merge_clause.h"
#include "parser/query/updating_clause/set_clause.h"

using namespace riag::parser;
using namespace riag::common;

TEST(NamespaceParserTest, DefinitionsAndSessionSelection) {
    auto statements = Parser::parseQuery("CREATE NAMESPACE IF NOT EXISTS `Team A`; "
                                         "USE NAMESPACE GLOBAL, `Team A`; "
                                         "DROP NAMESPACE IF EXISTS `Team A`");
    ASSERT_EQ(statements.size(), 3);
    const auto& create = statements[0]->constCast<NamespaceStatement>();
    EXPECT_EQ(create.action, NamespaceAction::CREATE);
    EXPECT_TRUE(create.ifExists);
    EXPECT_EQ(create.name, "Team A");
    const auto& use = statements[1]->constCast<NamespaceStatement>();
    EXPECT_EQ(use.action, NamespaceAction::USE);
    EXPECT_TRUE(use.selector.global);
    EXPECT_EQ(use.selector.names, std::vector<std::string>{"Team A"});
    EXPECT_EQ(statements[2]->constCast<NamespaceStatement>().action, NamespaceAction::DROP);
    auto all = Parser::parseQuery("USE NAMESPACE ALL NAMESPACES");
    EXPECT_TRUE(all[0]->constCast<NamespaceStatement>().selector.all);
}

TEST(NamespaceParserTest, RenameNamesSourceAndTarget) {
    auto statements = Parser::parseQuery("ALTER NAMESPACE `Team A` RENAME TO team_b; "
                                         "alter namespace global rename to `GLOBAL`");
    ASSERT_EQ(statements.size(), 2);
    const auto& rename = statements[0]->constCast<NamespaceStatement>();
    EXPECT_EQ(rename.action, NamespaceAction::RENAME);
    EXPECT_EQ(rename.name, "Team A");
    EXPECT_EQ(rename.newName, "team_b");
    EXPECT_FALSE(rename.ifExists);
    // GLOBAL parses as a name here; the catalog rejects it as source and target.
    const auto& global = statements[1]->constCast<NamespaceStatement>();
    EXPECT_EQ(global.action, NamespaceAction::RENAME);
    EXPECT_EQ(global.name, "global");
    EXPECT_EQ(global.newName, "GLOBAL");
    EXPECT_THROW(Parser::parseQuery("ALTER NAMESPACE IF EXISTS a RENAME TO b"), ParserException);
    EXPECT_THROW(Parser::parseQuery("ALTER NAMESPACE a RENAME TO b, c"), ParserException);
    EXPECT_THROW(Parser::parseQuery("ALTER NAMESPACE a RENAME b"), ParserException);
}

TEST(NamespaceParserTest, InlineAndOuterSelectorsStaySeparate) {
    auto statements = Parser::parseQuery("MATCH (a:Person {id: 36} IN team_a)"
                                         "-[r:Knows IN GLOBAL]->(b IN team_b) "
                                         "IN ALL NAMESPACES RETURN a, r, b");
    const auto* query = statements[0]->constCast<RegularQuery>().getSingleQuery(0);
    const auto& match = query->getReadingClause(0)->constCast<MatchClause>();
    ASSERT_TRUE(match.getNamespaceSelector().has_value());
    EXPECT_TRUE(match.getNamespaceSelector()->all);
    const auto& element = match.getPatternElementsRef()[0];
    const auto* node = element.getFirstNodePattern();
    ASSERT_TRUE(node->getNamespaceSelector().has_value());
    EXPECT_EQ(node->getNamespaceSelector()->names, std::vector<std::string>{"team_a"});
    EXPECT_EQ(node->getTableNames(), std::vector<std::string>{"Person"});
    EXPECT_EQ(node->getPropertyKeyVals().size(), 1);
    const auto* chain = element.getPatternElementChain(0);
    EXPECT_TRUE(chain->getRelPattern()->getNamespaceSelector()->global);
    EXPECT_EQ(chain->getNodePattern()->getNamespaceSelector()->names,
        std::vector<std::string>{"team_b"});
}

TEST(NamespaceParserTest, AnonymousRecursiveAndFiniteSelectors) {
    auto statements = Parser::parseQuery("MATCH (:Person IN GLOBAL, team_a)"
                                         "-[r:Knows*0..3 IN `Team B`]->(IN ALL NAMESPACES) "
                                         "RETURN r");
    const auto* query = statements[0]->constCast<RegularQuery>().getSingleQuery(0);
    const auto& match = query->getReadingClause(0)->constCast<MatchClause>();
    const auto& element = match.getPatternElementsRef()[0];
    EXPECT_TRUE(element.getFirstNodePattern()->getNamespaceSelector()->global);
    const auto* chain = element.getPatternElementChain(0);
    EXPECT_EQ(chain->getRelPattern()->getNamespaceSelector()->names,
        std::vector<std::string>{"Team B"});
    EXPECT_EQ(chain->getRelPattern()->getRecursiveInfo()->lowerBound, "0");
    EXPECT_TRUE(chain->getNodePattern()->getNamespaceSelector()->all);
}

TEST(NamespaceParserTest, SchemaPolicyAndPrimaryKeyScope) {
    auto statements = Parser::parseQuery("CREATE NODE TABLE Person IN GLOBAL, team_a "
                                         "(id INT64 LOCAL PRIMARY KEY, name STRING)");
    const auto* info = statements[0]->constCast<CreateTable>().getInfo();
    ASSERT_TRUE(info->namespaceSelector.has_value());
    EXPECT_TRUE(info->namespaceSelector->global);
    EXPECT_TRUE(info->extraInfo->constCast<ExtraCreateNodeTableInfo>().localPrimaryKey);
    auto tableConstraint =
        Parser::parseQuery("CREATE NODE TABLE Person (id INT64, LOCAL PRIMARY KEY(id))");
    EXPECT_TRUE(tableConstraint[0]
                    ->constCast<CreateTable>()
                    .getInfo()
                    ->extraInfo->constCast<ExtraCreateNodeTableInfo>()
                    .localPrimaryKey);
    EXPECT_THROW(Parser::parseQuery("CREATE NODE TABLE Person (id INT64 LOCAL PRIMARY KEY, "
                                    "GLOBAL PRIMARY KEY(id))"),
        ParserException);
    auto alter = Parser::parseQuery("ALTER TABLE Person SET NAMESPACES ALL NAMESPACES");
    EXPECT_EQ(alter[0]->constCast<Alter>().getInfo()->type, AlterType::SET_NAMESPACES);
}

TEST(NamespaceParserTest, WritesAndMovesPreserveTheirSelectors) {
    auto statements = Parser::parseQuery("CREATE (a:Person {id: 1} IN team_a) IN GLOBAL "
                                         "SET a IN team_b RETURN a");
    const auto* query = statements[0]->constCast<RegularQuery>().getSingleQuery(0);
    const auto& create = query->getUpdatingClause(0)->constCast<InsertClause>();
    EXPECT_TRUE(create.getNamespaceSelector()->global);
    const auto& set = query->getUpdatingClause(1)->constCast<SetClause>();
    ASSERT_EQ(set.getNamespaceMoves().size(), 1);
    EXPECT_EQ(set.getNamespaceMoves()[0].variable, "a");
    EXPECT_EQ(set.getNamespaceMoves()[0].destination.names, std::vector<std::string>{"team_b"});
    auto copy = Parser::parseQuery("COPY Person(id, name) IN team_a FROM 'people.csv'");
    EXPECT_EQ(copy[0]->constCast<CopyFrom>().namespaceSelector->names,
        std::vector<std::string>{"team_a"});
}

TEST(NamespaceParserTest, ExistingPatternsDoNotAcquireSelectors) {
    auto statements = Parser::parseQuery("MATCH (n:Person {id: 36})-[r:Knows]->(m) RETURN n");
    const auto* query = statements[0]->constCast<RegularQuery>().getSingleQuery(0);
    const auto& match = query->getReadingClause(0)->constCast<MatchClause>();
    EXPECT_FALSE(match.getNamespaceSelector().has_value());
    const auto& element = match.getPatternElementsRef()[0];
    EXPECT_FALSE(element.getFirstNodePattern()->getNamespaceSelector().has_value());
    EXPECT_FALSE(
        element.getPatternElementChain(0)->getRelPattern()->getNamespaceSelector().has_value());
}

TEST(NamespaceParserTest, InvalidSelectorPlacementAndExpressionsAreRejected) {
    EXPECT_THROW(Parser::parseQuery("MATCH (a IN 'team_a') RETURN a"), ParserException);
    EXPECT_THROW(Parser::parseQuery("MATCH (a IN $ns) RETURN a"), ParserException);
    EXPECT_THROW(Parser::parseQuery("MATCH (a IN team_a:Person) RETURN a"), ParserException);
    EXPECT_THROW(Parser::parseQuery("MATCH (a IN team_a IN team_b) RETURN a"), ParserException);
}

// A bare USE NAMESPACE is an empty selection, not USE <database named NAMESPACE> (RiaGraphDev#100).
TEST(NamespaceParserTest, BareUseNamespaceIsAnEmptySelection) {
    EXPECT_THROW(Parser::parseQuery("USE NAMESPACE"), ParserException);
    EXPECT_THROW(Parser::parseQuery("use namespace;"), ParserException);
    EXPECT_EQ(Parser::parseQuery("USE `namespace`")[0]->getStatementType(),
        StatementType::USE_DATABASE);
    EXPECT_EQ(Parser::parseQuery("USE NAMESPACE red")[0]->constCast<NamespaceStatement>().action,
        NamespaceAction::USE);
}

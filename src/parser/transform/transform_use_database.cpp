#include "common/exception/parser.h"
#include "parser/transformer.h"
#include "parser/use_database.h"

namespace riag {
namespace parser {

std::unique_ptr<Statement> Transformer::transformUseDatabase(
    CypherParser::IC_UseDatabaseContext& ctx) {
    // `USE NAMESPACE` without a list is not `USE <database named NAMESPACE>`: the unquoted
    // keyword after USE always starts a namespace selection, which must not be empty. A database
    // of that name stays reachable as `USE \`namespace\``.
    const auto& names = ctx.oC_SchemaName()->oC_SymbolicName();
    if (names.size() == 1 && names[0]->iC_NonReservedKeywords() != nullptr &&
        names[0]->iC_NonReservedKeywords()->NAMESPACE() != nullptr) {
        throw common::ParserException("USE NAMESPACE requires a namespace list or ALL NAMESPACES.");
    }
    auto dbName = transformSchemaName(*ctx.oC_SchemaName());
    return std::make_unique<UseDatabase>(std::move(dbName));
}

} // namespace parser
} // namespace riag

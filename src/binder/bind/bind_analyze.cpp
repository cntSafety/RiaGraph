#include "binder/binder.h"
#include "binder/bound_analyze.h"
#include "parser/analyze_statement.h"

using namespace riag::parser;

namespace riag {
namespace binder {

std::unique_ptr<BoundStatement> Binder::bindAnalyze(const Statement& statement) {
    auto& analyze = statement.constCast<AnalyzeStatement>();
    return std::make_unique<BoundAnalyze>(analyze.getTableName());
}

} // namespace binder
} // namespace riag

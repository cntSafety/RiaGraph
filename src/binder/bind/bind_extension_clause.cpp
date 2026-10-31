#include "binder/binder.h"
#include "extension/binder_extension.h"

using namespace riag::common;
using namespace riag::parser;

namespace riag {
namespace binder {

std::unique_ptr<BoundStatement> Binder::bindExtensionClause(const parser::Statement& statement) {
    for (auto& binderExtension : binderExtensions) {
        auto boundStatement = binderExtension->bind(statement);
        if (boundStatement) {
            return boundStatement;
        }
    }
    UNREACHABLE_CODE;
}

} // namespace binder
} // namespace riag

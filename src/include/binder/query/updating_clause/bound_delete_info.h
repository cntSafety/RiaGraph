#pragma once

#include "binder/expression/expression.h"
#include "common/enums/delete_type.h"
#include "common/enums/table_type.h"
#include "common/namespace_set.h"

namespace riag {
namespace binder {

struct BoundDeleteInfo {
    common::DeleteNodeType deleteType;
    common::TableType tableType;
    std::shared_ptr<Expression> pattern;
    common::NamespaceSet namespaceScope;

    BoundDeleteInfo(common::DeleteNodeType deleteType, common::TableType tableType,
        std::shared_ptr<Expression> pattern)
        : deleteType{deleteType}, tableType{tableType}, pattern{std::move(pattern)} {}
    EXPLICIT_COPY_DEFAULT_MOVE(BoundDeleteInfo);

    std::string toString() const { return "Delete " + pattern->toString(); }

private:
    BoundDeleteInfo(const BoundDeleteInfo& other)
        : deleteType{other.deleteType}, tableType{other.tableType}, pattern{other.pattern},
          namespaceScope{other.namespaceScope} {}
};

} // namespace binder
} // namespace riag

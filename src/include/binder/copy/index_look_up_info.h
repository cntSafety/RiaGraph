#pragma once

#include "binder/expression/expression.h"
#include "common/namespace_set.h"

namespace riag {
namespace binder {

struct IndexLookupInfo {
    bool namespaceAware = false;
    common::namespace_id_t namespaceTarget = common::GLOBAL_NAMESPACE_ID;
    common::table_id_t nodeTableID;
    std::shared_ptr<Expression> offset; // output
    std::shared_ptr<Expression> key;    // input
    expression_vector warningExprs;

    IndexLookupInfo(common::table_id_t nodeTableID, std::shared_ptr<Expression> offset,
        std::shared_ptr<Expression> key, expression_vector warningExprs = {})
        : nodeTableID{nodeTableID}, offset{std::move(offset)}, key{std::move(key)},
          warningExprs(std::move(warningExprs)) {}
    IndexLookupInfo(const IndexLookupInfo& other) = default;
};

} // namespace binder
} // namespace riag

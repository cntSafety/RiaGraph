#pragma once

#include "binder/expression/node_rel_expression.h"

namespace riag::binder {

struct BoundNamespaceMove {
    std::shared_ptr<NodeOrRelExpression> entity;
    common::namespace_id_t target;
};

} // namespace riag::binder

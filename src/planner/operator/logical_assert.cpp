#include "planner/operator/logical_assert.h"

#include "planner/operator/factorization/flatten_resolver.h"

namespace riag {
namespace planner {

// Same factorization as a filter: the condition is evaluated over at most one unflat group, so
// every row it stands for gets its own value.
f_group_pos_set LogicalAssert::getGroupsPosToFlatten() const {
    auto childSchema = children[0]->getSchema();
    return FlattenAllButOne::getGroupsPosToFlatten(condition, *childSchema);
}

} // namespace planner
} // namespace riag

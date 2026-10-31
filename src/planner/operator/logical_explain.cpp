#include "planner/operator/logical_explain.h"

using namespace riag::common;

namespace riag {
namespace planner {

void LogicalExplain::computeSchema() {
    switch (explainType) {
    case ExplainType::PROFILE:
        if (children[0]->getSchema()) {
            copyChildSchema(0);
        } else {
            createEmptySchema();
        }
        break;
    case ExplainType::PHYSICAL_PLAN:
    case ExplainType::LOGICAL_PLAN:
        createEmptySchema();
        break;
    default:
        UNREACHABLE_CODE;
    }
}

void LogicalExplain::computeFlatSchema() {
    computeSchema();
}

void LogicalExplain::computeFactorizedSchema() {
    computeSchema();
}

} // namespace planner
} // namespace riag

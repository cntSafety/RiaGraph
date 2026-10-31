#include "planner/operator/logical_unwind_deduplicate.h"

using namespace riag::binder;
using namespace riag::common;

namespace riag {
namespace planner {

void LogicalUnwindDeduplicate::computeFactorizedSchema() {
    copyChildSchema(0);
}

} // namespace planner
} // namespace riag

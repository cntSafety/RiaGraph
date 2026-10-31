#include "processor/operator/aggregate/base_aggregate_scan.h"

using namespace riag::common;
using namespace riag::function;

namespace riag {
namespace processor {

void BaseAggregateScan::initLocalStateInternal(ResultSet* resultSet,
    ExecutionContext* /*context*/) {
    aggregateVectors.clear();
    for (auto& dataPos : scanInfo.aggregatesPos) {
        auto valueVector = resultSet->getValueVector(dataPos);
        aggregateVectors.push_back(valueVector);
    }
}

} // namespace processor
} // namespace riag

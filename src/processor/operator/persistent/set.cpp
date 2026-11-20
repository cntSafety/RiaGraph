#include "processor/operator/persistent/set.h"

#include "binder/expression/expression_util.h"

namespace riag {
namespace processor {

void SetNodeProperty::initLocalStateInternal(ResultSet* resultSet, ExecutionContext* context) {
    for (auto& executor : executors) {
        executor->init(resultSet, context);
    }
}

bool SetNodeProperty::getNextTuplesInternal(ExecutionContext* context) {
    if (!children[0]->getNextTuple(context)) {
        return false;
    }
    for (auto i = 0u; i < executors.size(); ++i) {
        for (const auto j : columnsToRefresh[i]) {
            executors[j]->refreshColumn(context);
        }
        executors[i]->set(context);
    }
    return true;
}

void SetRelProperty::initLocalStateInternal(ResultSet* resultSet, ExecutionContext* context) {
    for (auto& executor : executors) {
        executor->init(resultSet, context);
    }
}

bool SetRelProperty::getNextTuplesInternal(ExecutionContext* context) {
    if (!children[0]->getNextTuple(context)) {
        return false;
    }
    for (auto i = 0u; i < executors.size(); ++i) {
        for (const auto j : columnsToRefresh[i]) {
            executors[j]->refreshColumn(context);
        }
        executors[i]->set(context);
    }
    return true;
}

std::string SetPropertyPrintInfo::toString() const {
    std::string result = "Properties: ";
    result += binder::ExpressionUtil::toString(expressions);
    return result;
}
} // namespace processor
} // namespace riag

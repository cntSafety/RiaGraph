#pragma once

#include "binder/query/updating_clause/bound_namespace_move.h"
#include "planner/operator/logical_operator.h"

namespace riag::planner {

class LogicalNamespaceMutation final : public LogicalOperator {
public:
    LogicalNamespaceMutation(std::vector<binder::BoundNamespaceMove> moves,
        std::shared_ptr<LogicalOperator> child)
        : LogicalOperator{LogicalOperatorType::NAMESPACE_MUTATION, std::move(child)},
          moves{std::move(moves)} {}

    std::vector<binder::BoundNamespaceMove> moves;
    void computeFactorizedSchema() override { copyChildSchema(0); }
    void computeFlatSchema() override { copyChildSchema(0); }
    std::string getExpressionsForPrinting() const override { return "namespace membership"; }
    std::unique_ptr<LogicalOperator> copy() override {
        return std::make_unique<LogicalNamespaceMutation>(moves, children[0]->copy());
    }
};

} // namespace riag::planner

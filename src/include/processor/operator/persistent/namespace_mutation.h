#pragma once

#include <deque>
#include <memory>
#include <unordered_map>
#include <vector>

#include "catalog/namespace_catalog.h"
#include "processor/operator/physical_operator.h"

namespace riag::processor {

struct NamespaceMutationInfo {
    DataPos idPosition;
    common::namespace_id_t target;
    catalog::Catalog* owner;
    bool relationship;
};

class NamespaceMutationExecutor {
public:
    explicit NamespaceMutationExecutor(std::vector<NamespaceMutationInfo> infos)
        : infos{std::move(infos)} {}
    void init(ResultSet* resultSet);
    void execute(ExecutionContext* context);
    const std::vector<NamespaceMutationInfo>& getInfos() const { return infos; }

private:
    std::vector<NamespaceMutationInfo> infos;
    std::vector<common::ValueVector*> vectors;
    // One writer map per info. A deque, not a vector: growing a vector relocates its elements,
    // and with MSVC's unordered_map (whose move constructor is not noexcept) that relocation
    // copies, which a map of unique_ptr cannot do. A deque never relocates its elements.
    std::deque<
        std::unordered_map<common::table_id_t, std::unique_ptr<catalog::NamespaceWriteState>>>
        writers;
};

// Installed only for statements which actually change namespace membership.
// The ordinary global-only Insert/Set/Merge operator trees are unchanged.
class NamespaceMutation final : public PhysicalOperator {
public:
    NamespaceMutation(std::vector<NamespaceMutationInfo> infos,
        std::unique_ptr<PhysicalOperator> child, physical_op_id id)
        : PhysicalOperator{PhysicalOperatorType::NAMESPACE_MUTATION, std::move(child), id,
              std::make_unique<OPPrintInfo>()},
          executor{std::move(infos)} {}

    void initLocalStateInternal(ResultSet* resultSet, ExecutionContext* context) override;
    bool getNextTuplesInternal(ExecutionContext* context) override;
    bool isParallel() const override { return false; }
    std::unique_ptr<PhysicalOperator> copy() override {
        return std::make_unique<NamespaceMutation>(executor.getInfos(), children[0]->copy(), id);
    }

private:
    NamespaceMutationExecutor executor;
};

} // namespace riag::processor

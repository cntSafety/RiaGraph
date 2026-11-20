#include "processor/operator/persistent/namespace_mutation.h"

#include "catalog/catalog.h"
#include "common/exception/runtime.h"
#include "processor/execution_context.h"
#include "processor/result/result_set.h"
#include "storage/partition_storage_registry.h"
#include "storage/storage_manager.h"
#include "storage/table/node_table.h"
#include "transaction/transaction.h"

namespace riag::processor {

void NamespaceMutation::initLocalStateInternal(ResultSet* resultSet, ExecutionContext*) {
    executor.init(resultSet);
}

void NamespaceMutationExecutor::init(ResultSet* resultSet) {
    vectors.clear();
    writers.clear();
    writers.resize(infos.size());
    for (const auto& info : infos) {
        vectors.push_back(resultSet->getValueVector(info.idPosition).get());
    }
}

bool NamespaceMutation::getNextTuplesInternal(ExecutionContext* context) {
    if (!children[0]->getNextTuple(context)) {
        return false;
    }
    executor.execute(context);
    return true;
}

void NamespaceMutationExecutor::execute(ExecutionContext* context) {
    auto* transaction = transaction::Transaction::Get(*context->clientContext);
    for (size_t i = 0; i < infos.size(); ++i) {
        const auto& info = infos[i];
        auto& vector = *vectors[i];
        auto& infoWriters = writers[i];
        const auto& selected = vector.state->getSelVector();
        for (common::sel_t row = 0; row < selected.getSelSize(); ++row) {
            const auto position = selected[row];
            if (vector.isNull(position)) {
                continue;
            }
            const auto id = vector.getValue<common::internalID_t>(position);
            auto& writer = infoWriters[id.tableID];
            if (!writer) {
                auto& namespaces = info.owner->getOrCreateNamespaces();
                const auto* policy = namespaces.getPolicy(transaction, id.tableID);
                if (policy && !policy->allowed.contains(info.target)) {
                    throw common::RuntimeException(
                        "The destination namespace is not allowed for this table.");
                }
                writer = std::make_unique<catalog::NamespaceWriteState>(&namespaces, transaction,
                    id.tableID, info.relationship);
            }
            if (!info.relationship) {
                auto* table = storage::PartitionStorageRegistry::resolveNodeTableByID(
                    context->clientContext, id.tableID, info.owner);
                table->moveNamespace(transaction, id.offset, info.target);
            }
            writer->set(id.offset, info.target);
        }
    }
}

} // namespace riag::processor

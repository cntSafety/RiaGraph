#include "processor/operator/simple/namespace_command.h"

#include "catalog/catalog.h"
#include "catalog/catalog_entry/node_table_catalog_entry.h"
#include "catalog/catalog_entry/rel_group_catalog_entry.h"
#include "catalog/namespace_catalog.h"
#include "common/exception/runtime.h"
#include "main/client_context.h"
#include "processor/execution_context.h"
#include "storage/buffer_manager/memory_manager.h"
#include "storage/partition_storage_registry.h"
#include "storage/storage_manager.h"
#include "storage/table/node_table.h"
#include "storage/table/rel_table.h"
#include "transaction/transaction_context.h"
#include "transaction/transaction_manager.h"

namespace riag::processor {

void NamespaceCommand::executeInternal(ExecutionContext* context) {
    auto& client = *context->clientContext;
    auto* catalog = catalog::Catalog::Get(client);
    auto* transaction = transaction::Transaction::Get(client);
    if (action != parser::NamespaceAction::USE) {
        transaction::TransactionManager::Get(client)->claimNamespaceDDL(client, *transaction);
    }
    switch (action) {
    case parser::NamespaceAction::CREATE:
        catalog->getOrCreateNamespaces().create(transaction, name, ifExists);
        break;
    case parser::NamespaceAction::DROP:
        catalog->getOrCreateNamespaces().drop(transaction, name, ifExists);
        break;
    case parser::NamespaceAction::RENAME:
        catalog->getOrCreateNamespaces().rename(transaction, name, newName);
        break;
    case parser::NamespaceAction::USE: {
        if (!transaction::TransactionContext::Get(client)->isAutoTransaction()) {
            throw common::RuntimeException(
                "USE NAMESPACE is not allowed inside an explicit transaction.");
        }
        auto scope = common::NamespaceSet{};
        if (!selector.all) {
            std::vector<common::namespace_id_t> ids;
            if (selector.global) {
                ids.push_back(common::GLOBAL_NAMESPACE_ID);
            }
            for (const auto& namespaceName : selector.names) {
                const auto* namespaces = catalog->getNamespaces();
                if (!namespaces) {
                    throw common::RuntimeException(
                        "Namespace " + namespaceName + " does not exist.");
                }
                ids.push_back(namespaces->resolve(transaction, namespaceName));
            }
            scope = common::NamespaceSet::finite(std::move(ids));
            if (scope.isEmpty()) {
                throw common::RuntimeException("Namespace selection must not be empty.");
            }
        }
        client.setNamespaceSelection(std::move(scope), catalog);
        break;
    }
    case parser::NamespaceAction::TABLE_ADD:
    case parser::NamespaceAction::TABLE_REMOVE:
    case parser::NamespaceAction::TABLE_SET: {
        auto* entry = catalog->getTableCatalogEntry(transaction, name);
        auto& namespaces = catalog->getOrCreateNamespaces();
        common::NamespaceSet requested;
        if (!selector.all) {
            std::vector<common::namespace_id_t> ids;
            if (selector.global) {
                ids.push_back(common::GLOBAL_NAMESPACE_ID);
            }
            for (const auto& item : selector.names) {
                ids.push_back(namespaces.resolve(transaction, item));
            }
            requested = common::NamespaceSet::finite(std::move(ids));
        }
        const auto* policy = namespaces.getPolicy(transaction, entry->getTableID());
        auto allowed = policy ? policy->allowed : common::NamespaceSet{};
        if (action == parser::NamespaceAction::TABLE_SET) {
            allowed = requested;
        } else if (action == parser::NamespaceAction::TABLE_ADD && allowed.isUnrestricted() &&
                   !requested.isUnrestricted()) {
            // ALL already contains every namespace, so adding names to it changes nothing once
            // they resolved above (namespace.md, section 4.3).
            break;
        } else {
            if (allowed.isUnrestricted() || requested.isUnrestricted()) {
                throw common::RuntimeException("ADD/REMOVE requires finite namespace policies; use "
                                               "SET NAMESPACES to replace ALL NAMESPACES.");
            }
            std::vector<common::namespace_id_t> ids;
            for (auto id : allowed.getIDs()) {
                if (action != parser::NamespaceAction::TABLE_REMOVE || !requested.contains(id)) {
                    ids.push_back(id);
                }
            }
            if (action == parser::NamespaceAction::TABLE_ADD) {
                ids.insert(ids.end(), requested.getIDs().begin(), requested.getIDs().end());
            }
            allowed = common::NamespaceSet::finite(std::move(ids));
        }
        if (allowed.isEmpty()) {
            throw common::RuntimeException("A table must allow at least one namespace.");
        }
        auto* storage =
            storage::PartitionStorageRegistry::resolveOwnerStorageManager(&client, catalog);
        const bool relationship = entry->getType() == catalog::CatalogEntryType::REL_GROUP_ENTRY;
        std::vector<common::table_id_t> tableIDs;
        if (relationship) {
            auto& rel = entry->constCast<catalog::RelGroupCatalogEntry>();
            if (rel.getScanFunction() || !rel.getStorage().empty() ||
                rel.getStorageFormat() != common::StorageFormat::NONE) {
                throw common::RuntimeException("Namespace policies require native tables.");
            }
            for (const auto& item : rel.getRelEntryInfos()) {
                auto* table = storage->getTable(item.oid)->ptrCast<storage::RelTable>();
                namespaces.validatePolicy(transaction, item.oid,
                    table->getNumVisibleRels(transaction, table->getStorageDirections().front()),
                    allowed);
                tableIDs.push_back(item.oid);
            }
        } else {
            auto& node = entry->constCast<catalog::NodeTableCatalogEntry>();
            if (node.isPartitioned() || node.isPartitionChild() || node.getScanFunction() ||
                !node.getStorage().empty() ||
                node.getStorageFormat() != common::StorageFormat::NONE) {
                throw common::RuntimeException(
                    "Namespace policies require unpartitioned native tables.");
            }
            auto* table = storage->getTable(entry->getTableID())->ptrCast<storage::NodeTable>();
            common::row_idx_t visible = 0;
            for (common::offset_t offset = 0, end = table->getNumTotalRows(transaction);
                 offset < end; ++offset) {
                visible += table->isVisible(transaction, offset);
            }
            namespaces.validatePolicy(transaction, entry->getTableID(), visible, allowed);
        }
        tableIDs.push_back(entry->getTableID());
        for (auto tableID : tableIDs) {
            const auto* old = namespaces.getPolicy(transaction, tableID);
            namespaces.setPolicy(transaction, tableID, relationship, allowed,
                old && old->localPrimaryKey, old && old->hasMembership);
        }
        break;
    }
    }
    appendMessage("Namespace command completed.", storage::MemoryManager::Get(client));
}

} // namespace riag::processor

#include "binder/binder.h"
#include "catalog/catalog.h"
#include "catalog/catalog_entry/node_table_catalog_entry.h"
#include "catalog/catalog_entry/rel_group_catalog_entry.h"
#include "catalog/namespace_catalog.h"
#include "common/string_utils.h"
#include "function/table/bind_data.h"
#include "function/table/bind_input.h"
#include "function/table/simple_table_function.h"
#include "processor/execution_context.h"
#include "storage/partition_storage_registry.h"
#include "storage/storage_manager.h"
#include "storage/table/node_table.h"
#include "storage/table/rel_table.h"
#include "transaction/transaction.h"

using namespace riag::common;

namespace riag::function {
namespace {
struct Row {
    std::string name;
    namespace_id_t id = GLOBAL_NAMESPACE_ID;
    std::string policy;
    std::string key;
    std::string storage;
    std::string counts;
};

struct BindData final : TableFuncBindData {
    bool details;
    std::vector<Row> rows;
    BindData(bool details, std::vector<Row> rows, binder::expression_vector columns)
        : TableFuncBindData{std::move(columns), rows.size()}, details{details},
          rows{std::move(rows)} {}
    std::unique_ptr<TableFuncBindData> copy() const override {
        return std::make_unique<BindData>(details, rows, columns);
    }
};

Row getInfo(const main::ClientContext* context, const std::string& tableName);

offset_t outputRows(const TableFuncMorsel& morsel, const TableFuncInput& input, DataChunk& output) {
    const auto& data = *input.bindData->constPtrCast<BindData>();
    for (uint64_t i = 0; i < morsel.getMorselSize(); ++i) {
        const auto& boundRow = data.rows[morsel.startOffset + i];
        const auto row =
            data.details ? getInfo(input.context->clientContext, boundRow.name) : boundRow;
        output.getValueVectorMutable(0).setValue(i, row.name);
        if (data.details) {
            output.getValueVectorMutable(1).setValue(i, row.policy);
            output.getValueVectorMutable(2).setValue(i, row.key);
            output.getValueVectorMutable(3).setValue(i, row.storage);
            output.getValueVectorMutable(4).setValue(i, row.counts);
        } else {
            output.getValueVectorMutable(1).setValue(i, row.id);
            output.getValueVectorMutable(2).setValue(i, row.id == GLOBAL_NAMESPACE_ID);
        }
    }
    return morsel.getMorselSize();
}

std::unique_ptr<TableFuncBindData> bindNamespaces(const main::ClientContext* context,
    const TableFuncBindInput* input) {
    std::vector<Row> rows{{"GLOBAL", GLOBAL_NAMESPACE_ID, "", "", "", ""}};
    const auto* namespaces = catalog::Catalog::Get(*context)->getNamespaces();
    if (namespaces) {
        for (const auto* entry :
            namespaces->getDefinitions(transaction::Transaction::Get(*context))) {
            rows.push_back({entry->getName().substr(2), entry->getOID(), "", "", "", ""});
        }
    }
    std::sort(rows.begin() + 1, rows.end(),
        [](const auto& left, const auto& right) { return left.id < right.id; });
    auto names =
        TableFunction::extractYieldVariables({"name", "id", "is_global"}, input->yieldVariables);
    std::vector<LogicalType> types;
    types.push_back(LogicalType::STRING());
    types.push_back(LogicalType::UINT64());
    types.push_back(LogicalType::BOOL());
    auto columns = input->binder->createVariables(names, types);
    return std::make_unique<BindData>(false, std::move(rows), std::move(columns));
}

Row getInfo(const main::ClientContext* context, const std::string& tableName) {
    auto* transaction = transaction::Transaction::Get(*context);
    auto* owner = catalog::Catalog::Get(*context);
    auto* table = owner->getTableCatalogEntry(transaction, tableName);
    const auto* namespaces = owner->getNamespaces();
    const auto* policy =
        namespaces ? namespaces->getPolicy(transaction, table->getTableID()) : nullptr;
    Row row;
    row.name = table->getName();
    row.policy = "ALL NAMESPACES";
    if (policy && !policy->allowed.isUnrestricted()) {
        row.policy.clear();
        for (auto id : policy->allowed.getIDs()) {
            if (!row.policy.empty()) {
                row.policy += ", ";
            }
            row.policy += id == GLOBAL_NAMESPACE_ID ?
                              "GLOBAL" :
                              "`" + namespaces->getName(transaction, id) + "`";
        }
    }
    row.key = table->getTableType() == TableType::NODE ?
                  (policy && policy->localPrimaryKey ? "LOCAL" : "GLOBAL") :
                  "NONE";
    bool materialized = policy && policy->hasMembership;
    if (namespaces && table->getType() == catalog::CatalogEntryType::REL_GROUP_ENTRY) {
        for (const auto& info :
            table->constCast<catalog::RelGroupCatalogEntry>().getRelEntryInfos()) {
            const auto* physicalPolicy = namespaces->getPolicy(transaction, info.oid);
            materialized |= physicalPolicy && physicalPolicy->hasMembership;
        }
    }
    row.storage = materialized ? "MVCC_GROUPS" : "IMPLICIT_GLOBAL";
    std::unordered_map<namespace_id_t, row_idx_t> counts;
    row_idx_t visible = 0;
    bool native = true;
    auto* storage = storage::StorageManager::Get(*context);
    const auto addCounts = [&](table_id_t id) {
        if (namespaces) {
            for (const auto& [namespaceID, count] :
                namespaces->countNamedMemberships(transaction, id)) {
                counts[namespaceID] += count;
            }
        }
    };
    if (table->getType() == catalog::CatalogEntryType::NODE_TABLE_ENTRY) {
        const auto& entry = table->constCast<catalog::NodeTableCatalogEntry>();
        native = !entry.isPartitioned() && !entry.isPartitionChild() && !entry.getScanFunction() &&
                 entry.getStorage().empty() && entry.getStorageFormat() == StorageFormat::NONE;
        if (native) {
            auto* nodes = storage->getTable(table->getTableID())->ptrCast<storage::NodeTable>();
            for (offset_t offset = 0, end = nodes->getNumTotalRows(transaction); offset < end;
                 ++offset) {
                visible += nodes->isVisible(transaction, offset);
            }
            addCounts(table->getTableID());
        }
    } else if (table->getType() == catalog::CatalogEntryType::REL_GROUP_ENTRY) {
        const auto& entry = table->constCast<catalog::RelGroupCatalogEntry>();
        native = !entry.getScanFunction() && entry.getStorage().empty() &&
                 entry.getStorageFormat() == StorageFormat::NONE;
        if (native) {
            for (const auto& info : entry.getRelEntryInfos()) {
                auto* rels = storage->getTable(info.oid)->ptrCast<storage::RelTable>();
                visible +=
                    rels->getNumVisibleRels(transaction, rels->getStorageDirections().front());
                addCounts(info.oid);
            }
        }
    } else {
        native = false;
    }
    if (native) {
        row_idx_t named = 0;
        for (const auto& [_, count] : counts) {
            named += count;
        }
        row.counts = "GLOBAL=" + std::to_string(visible > named ? visible - named : 0);
        if (namespaces) {
            auto definitions = namespaces->getDefinitions(transaction);
            std::sort(definitions.begin(), definitions.end(),
                [](const auto* a, const auto* b) { return a->getOID() < b->getOID(); });
            for (const auto* entry : definitions) {
                row.counts += ", " + StringUtils::quoteIdentifier(entry->getName().substr(2)) +
                              "=" + std::to_string(counts[entry->getOID()]);
            }
        }
    } else {
        row.counts = "UNAVAILABLE";
    }
    return row;
}

std::unique_ptr<TableFuncBindData> bindInfo(const main::ClientContext* context,
    const TableFuncBindInput* input) {
    Row row;
    row.name = catalog::Catalog::Get(*context)
                   ->getTableCatalogEntry(transaction::Transaction::Get(*context),
                       input->getLiteralVal<std::string>(0))
                   ->getName();
    auto names = TableFunction::extractYieldVariables({"table_name", "allowed_namespaces",
                                                          "primary_key_scope", "membership_storage",
                                                          "element_counts"},
        input->yieldVariables);
    std::vector<LogicalType> types;
    for (auto i = 0; i < 5; ++i) {
        types.push_back(LogicalType::STRING());
    }
    auto columns = input->binder->createVariables(names, types);
    return std::make_unique<BindData>(true, std::vector<Row>{std::move(row)}, std::move(columns));
}

function_set functions(const char* name, bool details) {
    function_set result;
    auto function = std::make_unique<TableFunction>(name,
        details ? std::vector{LogicalTypeID::STRING} : std::vector<LogicalTypeID>{});
    function->tableFunc = SimpleTableFunc::getTableFunc(outputRows);
    function->bindFunc = details ? bindInfo : bindNamespaces;
    function->initSharedStateFunc = SimpleTableFunc::initSharedState;
    function->initLocalStateFunc = TableFunction::initEmptyLocalState;
    result.push_back(std::move(function));
    return result;
}
} // namespace

function_set ShowNamespacesFunction::getFunctionSet() {
    return functions(name, false);
}
function_set NamespaceInfoFunction::getFunctionSet() {
    return functions(name, true);
}
} // namespace riag::function

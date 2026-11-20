#include "binder/expression/literal_expression.h"
#include "binder/expression/node_rel_expression.h"
#include "binder/expression/scalar_function_expression.h"
#include "binder/expression_binder.h"
#include "catalog/catalog.h"
#include "catalog/catalog_entry/node_table_catalog_entry.h"
#include "catalog/catalog_entry/rel_group_catalog_entry.h"
#include "catalog/namespace_catalog.h"
#include "common/exception/binder.h"
#include "common/exception/runtime.h"
#include "function/rewrite_function.h"
#include "function/scalar_function.h"
#include "function/schema/namespace_functions.h"
#include "function/struct/vector_struct_functions.h"
#include "main/client_context.h"
#include "storage/partition_storage_registry.h"
#include "storage/storage_manager.h"
#include "storage/table/table.h"
#include "transaction/transaction.h"
#include "transaction/transaction_context.h"
#include <format>

using namespace riag::common;
using namespace riag::binder;

namespace riag::function {
namespace {

using Owners = std::unordered_map<table_id_t, catalog::Catalog*>;

Owners getOwners(const NodeOrRelExpression& entity) {
    Owners owners;
    for (auto* entry : entity.getEntries()) {
        const auto add = [&](table_id_t id) {
            auto [it, inserted] = owners.emplace(id, entry->getOwningCatalog());
            if (!inserted && it->second != entry->getOwningCatalog()) {
                throw BinderException("Namespace selection cannot disambiguate overlapping table "
                                      "IDs across catalogs in one pattern.");
            }
        };
        if (entry->getType() == catalog::CatalogEntryType::REL_GROUP_ENTRY) {
            for (const auto& info :
                entry->constCast<catalog::RelGroupCatalogEntry>().getRelEntryInfos()) {
                add(info.oid);
            }
        } else {
            add(entry->getTableID());
        }
    }
    return owners;
}

bool isImplicitGlobal(const Owners& owners, main::ClientContext* context) {
    auto* transaction = transaction::Transaction::Get(*context);
    for (const auto& [id, owner] : owners) {
        if (const auto* namespaces = owner->getNamespaces()) {
            const auto* policy = namespaces->getPolicy(transaction, id);
            if (policy && policy->hasMembership) {
                return false;
            }
        }
    }
    return true;
}

struct NamespaceBindData final : FunctionBindData {
    NamespaceBindData(Owners owners, NamespaceSet scope, bool predicate,
        main::ClientContext* context, bool assertion = false)
        : FunctionBindData{predicate ? LogicalType::BOOL() : LogicalType::STRING()},
          owners{std::move(owners)}, scope{std::move(scope)}, predicate{predicate},
          assertion{assertion} {
        clientContext = context;
    }

    Owners owners;
    NamespaceSet scope;
    bool predicate;
    bool assertion;
    transaction_t transactionID = INVALID_TRANSACTION;
    std::unordered_map<table_id_t, std::unique_ptr<catalog::NamespaceReadState>> readers;
    table_id_t currentTable = INVALID_TABLE_ID;
    catalog::NamespaceReadState* currentReader = nullptr;
    std::unordered_map<table_id_t, std::unordered_map<namespace_id_t, std::string>> names;
    mutable std::optional<double> estimatedSelectivity;

    std::unique_ptr<FunctionBindData> copy() const override {
        return std::make_unique<NamespaceBindData>(owners, scope, predicate, clientContext,
            assertion);
    }

    transaction::Transaction* beginVector() {
        auto* transaction = transaction::Transaction::Get(*clientContext);
        if (transactionID != transaction->getID()) {
            readers.clear();
            names.clear();
            currentTable = INVALID_TABLE_ID;
            currentReader = nullptr;
            transactionID = transaction->getID();
        }
        return transaction;
    }

    namespace_id_t membership(internalID_t id, const transaction::Transaction* transaction) {
        if (currentReader && id.tableID == currentTable) {
            return currentReader->get(id.offset);
        }
        auto it = readers.find(id.tableID);
        if (it == readers.end()) {
            const auto owner = owners.find(id.tableID);
            if (owner == owners.end() || !owner->second) {
                throw RuntimeException("Cannot resolve namespace ownership for an element.");
            }
            it = readers
                     .emplace(id.tableID,
                         std::make_unique<catalog::NamespaceReadState>(
                             owner->second->getNamespaces(), transaction, id.tableID))
                     .first;
        }
        currentTable = id.tableID;
        currentReader = it->second.get();
        return currentReader->get(id.offset);
    }

    const std::string& namespaceName(internalID_t id, namespace_id_t namespaceID) {
        auto& tableNames = names[id.tableID];
        auto it = tableNames.find(namespaceID);
        if (it == tableNames.end()) {
            auto* namespaces = owners.at(id.tableID)->getNamespaces();
            it = tableNames
                     .emplace(namespaceID,
                         namespaces->getName(transaction::Transaction::Get(*clientContext),
                             namespaceID))
                     .first;
        }
        return it->second;
    }
};

void executeNamespace(const std::vector<std::shared_ptr<ValueVector>>& parameters,
    const std::vector<SelectionVector*>& selections, ValueVector& result,
    SelectionVector* resultSelection, void* pointer) {
    auto& data = *static_cast<NamespaceBindData*>(pointer);
    auto& input = *parameters[0];
    const auto* transaction = data.beginVector();
    if (!data.predicate) {
        result.resetAuxiliaryBuffer();
    }
    for (sel_t i = 0; i < selections[0]->getSelSize(); ++i) {
        const auto position = (*selections[0])[i];
        const auto output = (*resultSelection)[i];
        if (input.isNull(position)) {
            result.setNull(output, !data.assertion);
            if (data.assertion) {
                result.setValue<bool>(output, true);
            }
            continue;
        }
        const auto id = input.getValue<internalID_t>(position);
        const auto namespaceID = data.membership(id, transaction);
        if (data.predicate) {
            if (data.assertion && !data.scope.contains(namespaceID)) {
                throw RuntimeException(
                    "A bound element does not belong to its asserted namespace.");
            }
            result.setNull(output, false);
            result.setValue<bool>(output, data.scope.contains(namespaceID));
        } else if (namespaceID == GLOBAL_NAMESPACE_ID) {
            result.setNull(output, true);
        } else {
            result.setNull(output, false);
            StringVector::addString(&result, output, data.namespaceName(id, namespaceID));
        }
    }
}

bool selectNamespace(const std::vector<std::shared_ptr<ValueVector>>& parameters,
    SelectionVector& selection, void* pointer) {
    auto& data = *static_cast<NamespaceBindData*>(pointer);
    auto& input = *parameters[0];
    const auto* transaction = data.beginVector();
    const auto& candidates = input.state->getSelVector();
    auto output = selection.getMutableBuffer();
    sel_t count = 0;
    for (sel_t i = 0; i < candidates.getSelSize(); ++i) {
        const auto position = candidates[i];
        if (input.isNull(position)) {
            if (data.assertion) {
                output[count++] = position;
            }
            continue;
        }
        const bool matches = data.scope.contains(
            data.membership(input.getValue<internalID_t>(position), transaction));
        if (data.assertion && !matches) {
            throw RuntimeException("A bound element does not belong to its asserted namespace.");
        }
        if (matches) {
            output[count++] = position;
        }
    }
    selection.setToFiltered(count);
    return count != 0;
}

std::shared_ptr<Expression> makeExpression(std::shared_ptr<Expression> internalID, Owners owners,
    NamespaceSet scope, bool predicate, main::ClientContext* context, std::string uniqueName,
    bool assertion = false) {
    auto function =
        std::make_unique<ScalarFunction>(predicate ? "NAMESPACE_MATCH" : NamespaceFunction::name,
            std::vector<LogicalTypeID>{LogicalTypeID::INTERNAL_ID},
            predicate ? LogicalTypeID::BOOL : LogicalTypeID::STRING, executeNamespace,
            predicate ? selectNamespace : scalar_func_select_t{});
    auto data = std::make_unique<NamespaceBindData>(std::move(owners), std::move(scope), predicate,
        context, assertion);
    return std::make_shared<ScalarFunctionExpression>(ExpressionType::FUNCTION, std::move(function),
        std::move(data), expression_vector{std::move(internalID)}, std::move(uniqueName));
}

// namespace(x) of an element that is implicitly global is the constant NULL. A literal reads as
// its value, which for NULL is empty, so the constant reads as the function expression it stands
// for, NAMESPACE(x._ID), and names its column as that would. It is not given an alias: an alias
// would also let an unaliased WITH item pass, and only while the table has no named member.
std::shared_ptr<Expression> createGlobalNamespace(const ExpressionBinder& binder,
    const Expression& internalID) {
    auto value = Value::createNullValue(LogicalType::STRING());
    const auto uniqueName = binder.createNullLiteralExpression(value)->getUniqueName();
    return std::make_shared<LiteralExpression>(std::move(value), uniqueName,
        std::format("{}({})", NamespaceFunction::name, internalID.toString()));
}

} // namespace

std::shared_ptr<Expression> createNamespacePredicate(const NodeOrRelExpression& entity,
    NamespaceSet scope, main::ClientContext* context, std::string uniqueName, bool assertion) {
    if (scope.isUnrestricted()) {
        return nullptr;
    }
    const auto owners = getOwners(entity);
    const auto ids = scope.getIDs();
    if (std::any_of(ids.begin(), ids.end(),
            [](namespace_id_t id) { return id != GLOBAL_NAMESPACE_ID; })) {
        const auto* selectedOwner = catalog::Catalog::Get(*context);
        for (const auto& [_, owner] : owners) {
            if (owner != selectedOwner) {
                throw BinderException("Named namespace selectors require the element's owning "
                                      "catalog to be selected.");
            }
        }
    }
    if (!entity.namespaceMutable &&
        transaction::TransactionContext::Get(*context)->isAutoTransaction() &&
        isImplicitGlobal(owners, context)) {
        if (scope.contains(GLOBAL_NAMESPACE_ID)) {
            return nullptr;
        }
        if (!assertion) {
            return std::make_shared<LiteralExpression>(Value(false), std::move(uniqueName));
        }
    }
    return makeExpression(entity.getInternalID(), owners, std::move(scope), true, context,
        std::move(uniqueName), assertion);
}

std::shared_ptr<Expression> NamespaceFunction::rewriteFunc(const RewriteFunctionBindInput& input) {
    if (input.arguments.size() != 1) {
        throw BinderException("namespace() requires one node or relationship argument.");
    }
    auto& argument = *input.arguments[0];
    if (argument.expressionType == ExpressionType::LITERAL &&
        argument.constCast<LiteralExpression>().getValue().isNull()) {
        return input.expressionBinder->createNullLiteralExpression(
            Value::createNullValue(LogicalType::STRING()));
    }
    if (argument.getDataType().getLogicalTypeID() != LogicalTypeID::NODE &&
        argument.getDataType().getLogicalTypeID() != LogicalTypeID::REL) {
        throw BinderException("namespace() requires a node or relationship value.");
    }
    if (argument.expressionType != ExpressionType::PATTERN) {
        auto internalID = input.expressionBinder->bindScalarFunctionExpression(
            {input.arguments[0],
                input.expressionBinder->createLiteralExpression(InternalKeyword::ID)},
            StructExtractFunctions::name);
        auto* owner = catalog::Catalog::Get(*input.context);
        if (!owner->getNamespaces()) {
            return createGlobalNamespace(*input.expressionBinder, *internalID);
        }
        auto* transaction = transaction::Transaction::Get(*input.context);
        Owners owners;
        if (argument.getDataType().getLogicalTypeID() == LogicalTypeID::NODE) {
            for (auto* entry : owner->getNodeTableEntries(transaction, false)) {
                owners.emplace(entry->getTableID(), owner);
            }
        } else {
            for (auto* entry : owner->getRelGroupEntries(transaction, false)) {
                for (const auto& info : entry->getRelEntryInfos()) {
                    owners.emplace(info.oid, owner);
                }
            }
        }
        return makeExpression(internalID, std::move(owners), NamespaceSet{}, false, input.context,
            ScalarFunctionExpression::getUniqueName(name, {internalID}));
    }
    auto& entity = argument.constCast<NodeOrRelExpression>();
    const auto owners = getOwners(entity);
    if (!entity.namespaceMutable &&
        transaction::TransactionContext::Get(*input.context)->isAutoTransaction() &&
        isImplicitGlobal(owners, input.context)) {
        return createGlobalNamespace(*input.expressionBinder, *entity.getInternalID());
    }
    return makeExpression(entity.getInternalID(), owners, NamespaceSet{}, false, input.context,
        ScalarFunctionExpression::getUniqueName(name, {entity.getInternalID()}));
}

std::optional<NamespaceSet> getNamespacePredicateScope(const Expression& predicate,
    const Expression& internalID) {
    if (predicate.expressionType != ExpressionType::FUNCTION) {
        return std::nullopt;
    }
    const auto& function = predicate.constCast<ScalarFunctionExpression>();
    if (function.getFunction().name != "NAMESPACE_MATCH" ||
        function.getChild(0)->getUniqueName() != internalID.getUniqueName()) {
        return std::nullopt;
    }
    const auto* data = static_cast<const NamespaceBindData*>(function.getBindData());
    return data->assertion ? std::nullopt : std::optional{data->scope};
}

std::optional<double> getNamespacePredicateSelectivity(const Expression& predicate) {
    if (predicate.expressionType != ExpressionType::FUNCTION) {
        return std::nullopt;
    }
    const auto& expression = predicate.constCast<ScalarFunctionExpression>();
    if (expression.getFunction().name != "NAMESPACE_MATCH") {
        return std::nullopt;
    }
    const auto& data = *static_cast<const NamespaceBindData*>(expression.getBindData());
    if (data.assertion) {
        return std::nullopt;
    }
    if (data.estimatedSelectivity) {
        return data.estimatedSelectivity;
    }
    uint64_t rows = 0;
    uint64_t selected = 0;
    auto* transaction = transaction::Transaction::Get(*data.clientContext);
    for (const auto& [tableID, owner] : data.owners) {
        auto* storage = storage::PartitionStorageRegistry::resolveOwnerStorageManager(
            data.clientContext, owner);
        const auto count = storage->getTable(tableID)->getNumTotalRows(transaction);
        rows += count;
        const auto* namespaces = owner->getNamespaces();
        if (!data.scope.contains(GLOBAL_NAMESPACE_ID)) {
            selected += namespaces ? namespaces->countMembers(transaction, tableID, data.scope) : 0;
            continue;
        }
        uint64_t named = 0;
        if (namespaces) {
            for (const auto& [id, members] :
                namespaces->countNamedMemberships(transaction, tableID)) {
                named += members;
                if (data.scope.contains(id)) {
                    selected += members;
                }
            }
        }
        if (data.scope.contains(GLOBAL_NAMESPACE_ID)) {
            selected += count > named ? count - named : 0;
        }
    }
    data.estimatedSelectivity = rows == 0 ? 0.0 : std::min(1.0, double(selected) / double(rows));
    return data.estimatedSelectivity;
}

function_set NamespaceFunction::getFunctionSet() {
    function_set functions;
    for (const auto type : {LogicalTypeID::NODE, LogicalTypeID::REL, LogicalTypeID::ANY}) {
        functions.push_back(
            std::make_unique<RewriteFunction>(name, std::vector<LogicalTypeID>{type}, rewriteFunc));
    }
    return functions;
}

} // namespace riag::function

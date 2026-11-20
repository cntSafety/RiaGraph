#include <array>
#include <set>

#include "binder/bound_export_database.h"
#include "binder/query/bound_regular_query.h"
#include "catalog/catalog.h"
#include "catalog/catalog_entry/index_catalog_entry.h"
#include "catalog/catalog_entry/node_table_catalog_entry.h"
#include "catalog/catalog_entry/rel_group_catalog_entry.h"
#include "catalog/namespace_catalog.h"
#include "common/copier_config/csv_reader_config.h"
#include "common/exception/binder.h"
#include "common/file_system/virtual_file_system.h"
#include "common/string_utils.h"
#include "main/client_context.h"
#include "parser/parser.h"
#include "parser/port_db.h"
#include "parser/query/regular_query.h"
#include "storage/storage_manager.h"
#include "storage/table/node_table.h"
#include "storage/table/rel_table.h"
#include "transaction/transaction.h"
#include <format>

using namespace riag::binder;
using namespace riag::common;
using namespace riag::parser;
using namespace riag::catalog;
using namespace riag::transaction;
using namespace riag::storage;

namespace riag {
namespace binder {

FileTypeInfo getFileType(case_insensitive_map_t<Value>& options) {
    auto fileTypeInfo =
        FileTypeInfo{FileType::PARQUET, PortDBConstants::DEFAULT_EXPORT_FORMAT_OPTION};
    if (options.contains(PortDBConstants::EXPORT_FORMAT_OPTION)) {
        auto value = options.at(PortDBConstants::EXPORT_FORMAT_OPTION);
        if (value.getDataType().getLogicalTypeID() != LogicalTypeID::STRING) {
            throw BinderException("The type of format option must be a string.");
        }
        auto valueStr = value.getValue<std::string>();
        StringUtils::toUpper(valueStr);
        fileTypeInfo = FileTypeInfo{FileTypeUtils::fromString(valueStr), valueStr};
        options.erase(PortDBConstants::EXPORT_FORMAT_OPTION);
    }
    return fileTypeInfo;
}

void bindExportTableData(ExportedTableData& tableData, const std::string& query,
    main::ClientContext* context, Binder* binder) {
    auto parsedStatement = Parser::parseQuery(query);
    DASSERT(parsedStatement.size() == 1);
    auto parsedQuery = parsedStatement[0]->constPtrCast<RegularQuery>();
    context->setUseInternalCatalogEntry(true /* useInternalCatalogEntry */);
    auto boundQuery = binder->bindQuery(*parsedQuery);
    context->setUseInternalCatalogEntry(false /* useInternalCatalogEntry */);
    auto columns = boundQuery->getStatementResult()->getColumns();
    for (auto& column : columns) {
        auto columnName = column->hasAlias() ? column->getAlias() : column->toString();
        tableData.columnNames.push_back(columnName);
        tableData.columnTypes.push_back(column->getDataType().copy());
    }
    tableData.regularQuery = std::move(boundQuery);
}

static std::string getExportNodeTableDataQuery(const TableCatalogEntry& entry) {
    return std::format("match (a:{}) return a.*", StringUtils::quoteIdentifier(entry.getName()));
}

static std::string getExportRelTableDataQuery(const TableCatalogEntry& relGroupEntry,
    const NodeTableCatalogEntry& srcEntry, const NodeTableCatalogEntry& dstEntry) {
    return std::format("match (a:{})-[r:{}]->(b:{}) return a.{},b.{},r.*;",
        StringUtils::quoteIdentifier(srcEntry.getName()),
        StringUtils::quoteIdentifier(relGroupEntry.getName()),
        StringUtils::quoteIdentifier(dstEntry.getName()),
        StringUtils::quoteIdentifier(srcEntry.getPrimaryKeyName()),
        StringUtils::quoteIdentifier(dstEntry.getPrimaryKeyName()));
}

static std::vector<ExportedTableData> getNamespaceExportInfo(const Catalog& catalog,
    main::ClientContext* context, Binder* binder, const FileTypeInfo& fileTypeInfo,
    const case_insensitive_map_t<Value>& options) {
    auto* transaction = Transaction::Get(*context);
    const auto& namespaces = *catalog.getNamespaces();
    const auto selector = [&](namespace_id_t id) {
        return id == GLOBAL_NAMESPACE_ID ?
                   std::string("GLOBAL") :
                   StringUtils::quoteIdentifier(namespaces.getName(transaction, id));
    };
    const auto copyOptions =
        fileTypeInfo.fileType == FileType::CSV ?
            CSVOption::toCypher(CSVReaderConfig::construct(options).option.toOptionsMap(false)) :
            "";
    const auto extension = StringUtils::getLower(fileTypeInfo.fileTypeStr);
    std::vector<ExportedTableData> result;
    for (const auto* table : catalog.getNodeTableEntries(transaction, false)) {
        for (auto id : namespaces.getActualNamespaces(transaction, table->getTableID())) {
            const auto* policy = namespaces.getPolicy(transaction, table->getTableID());
            if (policy && !policy->allowed.contains(id)) {
                continue;
            }
            ExportedTableData data;
            data.tableName = table->getName();
            data.fileName = std::format("ns_node_{}_{}.{}", table->getTableID(), id, extension);
            const auto query = std::format("MATCH (n:{} IN {}) IN ALL NAMESPACES RETURN n.*",
                StringUtils::quoteIdentifier(table->getName()), selector(id));
            bindExportTableData(data, query, context, binder);
            std::string columns;
            for (const auto& property : table->getProperties()) {
                if (!columns.empty()) {
                    columns += ", ";
                }
                columns += StringUtils::quoteIdentifier(property.getName());
            }
            data.namespaceImport =
                std::format("COPY {} ({}) IN {} FROM \"${{RIA_NAMESPACE_EXPORT}}/{}\" {};\n",
                    StringUtils::quoteIdentifier(table->getName()), columns, selector(id),
                    data.fileName, copyOptions);
            result.push_back(std::move(data));
        }
    }
    auto* storage = StorageManager::Get(*context);
    auto* memory = MemoryManager::Get(*context);
    for (const auto* group : catalog.getRelGroupEntries(transaction, false)) {
        for (const auto& rel : group->getRelEntryInfos()) {
            auto* table = storage->getTable(rel.oid)->ptrCast<RelTable>();
            const auto direction = table->getStorageDirections().front();
            const auto forward = direction == RelDataDirection::FWD;
            const auto boundTableID = forward ? rel.nodePair.srcTableID : rel.nodePair.dstTableID;
            const auto neighborTableID =
                forward ? rel.nodePair.dstTableID : rel.nodePair.srcTableID;
            auto* boundTable = storage->getTable(boundTableID)->ptrCast<NodeTable>();
            auto state = std::make_shared<DataChunkState>();
            ValueVector bound(LogicalType::INTERNAL_ID(), memory,
                std::make_shared<DataChunkState>());
            bound.state->setToFlat();
            bound.state->getSelVectorUnsafe().setToUnfiltered(1);
            ValueVector neighbor(LogicalType::INTERNAL_ID(), memory, state);
            ValueVector edge(LogicalType::INTERNAL_ID(), memory, state);
            RelTableScanState scan(*memory, &bound, {&neighbor, &edge}, state, true);
            scan.setToTable(transaction, table, {NBR_ID_COLUMN_ID, REL_ID_COLUMN_ID}, {},
                direction);
            NamespaceReadState boundNS(&namespaces, transaction, boundTableID);
            NamespaceReadState neighborNS(&namespaces, transaction, neighborTableID);
            NamespaceReadState edgeNS(&namespaces, transaction, rel.oid);
            std::set<std::array<namespace_id_t, 3>> triples;
            for (offset_t offset = 0, end = boundTable->getNumTotalRows(transaction); offset < end;
                 ++offset) {
                bound.setValue<nodeID_t>(0, {offset, boundTableID});
                table->initScanState(transaction, scan);
                while (table->scan(transaction, scan)) {
                    const auto& selected = state->getSelVector();
                    for (sel_t row = 0; row < selected.getSelSize(); ++row) {
                        const auto pos = selected[row];
                        const auto a = boundNS.get(offset);
                        const auto b = neighborNS.get(neighbor.readNodeOffset(pos));
                        triples.insert({forward ? a : b, edgeNS.get(edge.readNodeOffset(pos)),
                            forward ? b : a});
                    }
                }
            }
            const auto& source = catalog.getTableCatalogEntry(transaction, rel.nodePair.srcTableID)
                                     ->constCast<NodeTableCatalogEntry>();
            const auto& destination =
                catalog.getTableCatalogEntry(transaction, rel.nodePair.dstTableID)
                    ->constCast<NodeTableCatalogEntry>();
            for (const auto& ids : triples) {
                ExportedTableData data;
                data.tableName = group->getName();
                data.fileName = std::format("ns_rel_{}_{}_{}_{}.{}", rel.oid, ids[0], ids[1],
                    ids[2], extension);
                auto query = std::format("MATCH (a:{} IN {})-[r:{} IN {}]->(b:{} IN {}) IN ALL "
                                         "NAMESPACES RETURN a.{} AS _from, b.{} AS _to",
                    StringUtils::quoteIdentifier(source.getName()), selector(ids[0]),
                    StringUtils::quoteIdentifier(group->getName()), selector(ids[1]),
                    StringUtils::quoteIdentifier(destination.getName()), selector(ids[2]),
                    StringUtils::quoteIdentifier(source.getPrimaryKeyName()),
                    StringUtils::quoteIdentifier(destination.getPrimaryKeyName()));
                auto headers = std::format("_from {}, _to {}",
                    source.getPrimaryKeyDefinition().getType().toString(),
                    destination.getPrimaryKeyDefinition().getType().toString());
                std::string properties;
                uint64_t propertyIndex = 0;
                for (const auto& property : group->getProperties()) {
                    if (property.getType() == LogicalType::INTERNAL_ID()) {
                        continue;
                    }
                    auto alias = "c" + std::to_string(propertyIndex++);
                    query += std::format(", r.{} AS {}",
                        StringUtils::quoteIdentifier(property.getName()), alias);
                    headers += std::format(", {} {}", alias, property.getType().toString());
                    if (!properties.empty()) {
                        properties += ", ";
                    }
                    properties += StringUtils::quoteIdentifier(property.getName()) + ":" + alias;
                }
                bindExportTableData(data, query, context, binder);
                data.namespaceImport =
                    std::format("LOAD WITH HEADERS ({}) FROM \"${{RIA_NAMESPACE_EXPORT}}/{}\" {} "
                                "MATCH (a:{} {{{}:_from}} IN {}), (b:{} {{{}:_to}} IN {}) IN ALL "
                                "NAMESPACES CREATE (a)-[:{} {} IN {}]->(b);\n",
                        headers, data.fileName, copyOptions,
                        StringUtils::quoteIdentifier(source.getName()),
                        StringUtils::quoteIdentifier(source.getPrimaryKeyName()), selector(ids[0]),
                        StringUtils::quoteIdentifier(destination.getName()),
                        StringUtils::quoteIdentifier(destination.getPrimaryKeyName()),
                        selector(ids[2]), StringUtils::quoteIdentifier(group->getName()),
                        properties.empty() ? "" : "{" + properties + "}", selector(ids[1]));
                result.push_back(std::move(data));
            }
        }
    }
    for (const auto* index : catalog.getIndexEntries(transaction)) {
        const auto* table = index->getTableEntryToExport(context);
        if (!table) {
            continue;
        }
        ExportedTableData data;
        data.tableName = table->getName();
        data.fileName = table->getName() + "." + extension;
        bindExportTableData(data,
            std::format("MATCH (n:{}) IN ALL NAMESPACES RETURN n.*",
                StringUtils::quoteIdentifier(table->getName())),
            context, binder);
        result.push_back(std::move(data));
    }
    return result;
}

static std::vector<ExportedTableData> getExportInfo(const Catalog& catalog,
    main::ClientContext* context, Binder* binder, FileTypeInfo& fileTypeInfo) {
    auto transaction = Transaction::Get(*context);
    std::vector<ExportedTableData> exportData;
    for (auto entry : catalog.getNodeTableEntries(transaction, false /*useInternal*/)) {
        ExportedTableData tableData;
        tableData.tableName = entry->getName();
        tableData.fileName =
            entry->getName() + "." + StringUtils::getLower(fileTypeInfo.fileTypeStr);
        auto query = getExportNodeTableDataQuery(*entry);
        bindExportTableData(tableData, query, context, binder);
        exportData.push_back(std::move(tableData));
    }
    for (auto entry : catalog.getRelGroupEntries(transaction, false /* useInternal */)) {
        auto& relGroupEntry = entry->constCast<RelGroupCatalogEntry>();
        for (auto& info : relGroupEntry.getRelEntryInfos()) {
            ExportedTableData tableData;
            auto srcTableID = info.nodePair.srcTableID;
            auto dstTableID = info.nodePair.dstTableID;
            auto& srcEntry = catalog.getTableCatalogEntry(transaction, srcTableID)
                                 ->constCast<NodeTableCatalogEntry>();
            auto& dstEntry = catalog.getTableCatalogEntry(transaction, dstTableID)
                                 ->constCast<NodeTableCatalogEntry>();
            tableData.tableName = entry->getName();
            tableData.fileName =
                std::format("{}_{}_{}.{}", relGroupEntry.getName(), srcEntry.getName(),
                    dstEntry.getName(), StringUtils::getLower(fileTypeInfo.fileTypeStr));
            auto query = getExportRelTableDataQuery(relGroupEntry, srcEntry, dstEntry);
            bindExportTableData(tableData, query, context, binder);
            exportData.push_back(std::move(tableData));
        }
    }

    for (auto indexEntry : catalog.getIndexEntries(transaction)) {
        // Export
        ExportedTableData tableData;
        auto entry = indexEntry->getTableEntryToExport(context);
        if (entry == nullptr) {
            continue;
        }
        DASSERT(entry->getTableType() == TableType::NODE);
        tableData.tableName = entry->getName();
        tableData.fileName =
            entry->getName() + "." + StringUtils::getLower(fileTypeInfo.fileTypeStr);
        auto query = getExportNodeTableDataQuery(*entry);
        bindExportTableData(tableData, query, context, binder);
        exportData.push_back(std::move(tableData));
    }
    return exportData;
}

static bool schemaOnly(case_insensitive_map_t<Value>& parsedOptions,
    const parser::ExportDB& exportDB) {
    auto isSchemaOnlyOption = [](const std::pair<std::string, Value>& option) -> bool {
        if (option.first != PortDBConstants::SCHEMA_ONLY_OPTION) {
            return false;
        }
        if (option.second.getDataType() != LogicalType::BOOL()) {
            throw common::BinderException{std::format("The '{}' option must have a BOOL value.",
                PortDBConstants::SCHEMA_ONLY_OPTION)};
        }
        return option.second.getValue<bool>();
    };
    auto exportSchemaOnly =
        std::count_if(parsedOptions.begin(), parsedOptions.end(), isSchemaOnlyOption) != 0;
    if (exportSchemaOnly && exportDB.getParsingOptionsRef().size() != 1) {
        throw common::BinderException{std::format("When '{}' option is set to true in export "
                                                  "database, no other options are allowed.",
            PortDBConstants::SCHEMA_ONLY_OPTION)};
    }
    parsedOptions.erase(PortDBConstants::SCHEMA_ONLY_OPTION);
    return exportSchemaOnly;
}

std::unique_ptr<BoundStatement> Binder::bindExportDatabaseClause(const Statement& statement) {
    auto& exportDB = statement.constCast<ExportDB>();
    auto parsedOptions = bindParsingOptions(exportDB.getParsingOptionsRef());
    auto fileTypeInfo = getFileType(parsedOptions);
    switch (fileTypeInfo.fileType) {
    case FileType::CSV:
    case FileType::PARQUET:
        break;
    default:
        throw BinderException("Export database currently only supports csv and parquet files.");
    }
    auto exportSchemaOnly = schemaOnly(parsedOptions, exportDB);
    if (!exportSchemaOnly && fileTypeInfo.fileType != FileType::CSV && parsedOptions.size() != 0) {
        throw BinderException{"Only export to csv can have options."};
    }
    const auto* catalog = Catalog::Get(*clientContext);
    const auto namespaceAware = catalog->getNamespaces() != nullptr;
    auto exportData = exportSchemaOnly ? std::vector<ExportedTableData>{} :
                      namespaceAware   ? getNamespaceExportInfo(*catalog, clientContext, this,
                                             fileTypeInfo, parsedOptions) :
                                         getExportInfo(*catalog, clientContext, this, fileTypeInfo);
    auto boundFilePath = VirtualFileSystem::GetUnsafe(*clientContext)
                             ->expandPath(clientContext, exportDB.getFilePath());
    auto result = std::make_unique<BoundExportDatabase>(boundFilePath, fileTypeInfo,
        std::move(exportData), std::move(parsedOptions), exportSchemaOnly);
    result->namespaceAware = namespaceAware;
    return result;
}

} // namespace binder
} // namespace riag

#include "processor/operator/simple/attach_database.h"

#include "common/constants.h"
#include "common/exception/runtime.h"
#include "common/string_utils.h"
#include "main/attached_database.h"
#include "main/client_context.h"
#include "main/database.h"
#include "main/database_manager.h"
#include "processor/execution_context.h"
#include "storage/storage_extension.h"
#include "storage/storage_manager.h"
#include <format>

namespace riag {
namespace processor {

std::string AttachDatabasePrintInfo::toString() const {
    std::string result = "Database: ";
    if (!dbName.empty()) {
        result += dbName;
    } else {
        result += dbPath;
    }
    return result;
}

static std::string attachMessage() {
    return "Attached database successfully.";
}

void AttachDatabase::executeInternal(ExecutionContext* context) {
    auto client = context->clientContext;
    auto databaseManager = main::DatabaseManager::Get(*client);
    auto memoryManager = storage::MemoryManager::Get(*client);
    if (common::isRiaGDBType(common::StringUtils::getUpper(attachInfo.dbType))) {
        auto db = std::make_unique<main::AttachedRiaGDatabase>(attachInfo.dbPath,
            attachInfo.dbAlias, common::ATTACHED_RIAG_DB_TYPE, client);
        databaseManager->registerAttachedDatabase(std::move(db));
        client->addDBDirToFileSearchPath(attachInfo.dbPath);
        appendMessage(attachMessage(), memoryManager);
        return;
    }
    for (auto& storageExtension : client->getDatabase()->getStorageExtensions()) {
        if (storageExtension->canHandleDB(attachInfo.dbType)) {
            auto db = storageExtension->attach(attachInfo.dbAlias, attachInfo.dbPath, client,
                attachInfo.options);
            databaseManager->registerAttachedDatabase(std::move(db));
            client->addDBDirToFileSearchPath(attachInfo.dbPath);
            appendMessage(attachMessage(), memoryManager);
            return;
        }
    }
    auto errMsg =
        std::format("No loaded extension can handle database type: {}.", attachInfo.dbType);
    auto dbType = common::StringUtils::getLower(attachInfo.dbType);
    if (dbType == "adbc" || dbType == "duckdb" || dbType == "postgres" || dbType == "sqlite") {
        errMsg += std::format("\nDid you forget to load {} extension?\nYou can load it by: load "
                              "extension {};",
            dbType, dbType);
    }
    throw common::RuntimeException{errMsg};
}

} // namespace processor
} // namespace riag

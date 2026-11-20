#include "storage/database_header.h"

#include <cstring>

#include "common/exception/runtime.h"
#include "common/file_system/file_info.h"
#include "common/serializer/buffer_writer.h"
#include "common/serializer/buffered_file.h"
#include "common/serializer/deserializer.h"
#include "common/serializer/serializer.h"
#include "common/system_config.h"
#include "main/client_context.h"
#include "storage/page_manager.h"
#include "storage/storage_version_info.h"
#include <format>

namespace riag::storage {
static storage_version_t validateStorageVersion(common::Deserializer& deSer) {
    std::string key;
    deSer.validateDebuggingInfo(key, "storage_version");
    storage_version_t savedStorageVersion = 0;
    deSer.deserializeValue(savedStorageVersion);
    const auto storageVersion = StorageVersionInfo::getStorageVersion();
    if (!StorageVersionInfo::canReadStorageVersion(savedStorageVersion)) {
        // TODO(Guodong): Add a test case for this.
        throw common::RuntimeException(
            std::format("Trying to read a database file with a different version. "
                        "Database file version: {}, Current build storage version: {}",
                savedStorageVersion, storageVersion));
    }
    deSer.setStorageVersion(savedStorageVersion);
    return savedStorageVersion;
}

static void validateMagicBytes(common::Deserializer& deSer) {
    std::string key;
    deSer.validateDebuggingInfo(key, "magic");
    const auto numMagicBytes = StorageVersionInfo::NUM_MAGIC_BYTES;
    uint8_t magicBytes[StorageVersionInfo::NUM_MAGIC_BYTES];
    for (auto i = 0u; i < numMagicBytes; i++) {
        deSer.deserializeValue<uint8_t>(magicBytes[i]);
    }
    // Accept a header written before the fork as well as our own, so an existing
    // ladybug database opens in place. // riag-rename:skip upstream name
    // Serialization always writes MAGIC_BYTES, so the upgrade is one-way.
    if (memcmp(magicBytes, StorageVersionInfo::MAGIC_BYTES, numMagicBytes) != 0 &&
        memcmp(magicBytes, StorageVersionInfo::LEGACY_MAGIC_BYTES, numMagicBytes) != 0) {
        throw common::RuntimeException(
            "Unable to open database. The file is not a valid RiaG database file!");
    }
}

void DatabaseHeader::updateCatalogPageRange(PageManager& pageManager, PageRange newPageRange) {
    if (catalogPageRange.startPageIdx != common::INVALID_PAGE_IDX) {
        pageManager.freePageRange(catalogPageRange);
    }
    catalogPageRange = newPageRange;
}

void DatabaseHeader::freeMetadataPageRange(PageManager& pageManager) const {
    if (metadataPageRange.startPageIdx != common::INVALID_PAGE_IDX) {
        pageManager.freePageRange(metadataPageRange);
    }
}

static constexpr uint8_t HEADER_FORMAT_VERSION_WITH_DATAFILE_NUM_PAGES = 2;
static constexpr uint8_t HEADER_FORMAT_VERSION_WITH_CATALOG_VERSION = 3;

static void writeMagicBytes(common::Serializer& serializer) {
    serializer.writeDebuggingInfo("magic");
    // Always the current magic, never the legacy spelling.
    const auto numMagicBytes = StorageVersionInfo::NUM_MAGIC_BYTES;
    for (auto i = 0u; i < numMagicBytes; i++) {
        serializer.serializeValue<uint8_t>(StorageVersionInfo::MAGIC_BYTES[i]);
    }
}

void DatabaseHeader::serialize(common::Serializer& ser) const {
    writeMagicBytes(ser);
    ser.writeDebuggingInfo("storage_version");
    ser.serializeValue(storageVersion);
    ser.writeDebuggingInfo("catalog");
    ser.serializeValue(catalogPageRange.startPageIdx);
    ser.serializeValue(catalogPageRange.numPages);
    ser.writeDebuggingInfo("metadata");
    ser.serializeValue(metadataPageRange.startPageIdx);
    ser.serializeValue(metadataPageRange.numPages);
    ser.writeDebuggingInfo("databaseID");
    ser.serializeValue(databaseID.value);
    // No debugging info so old readers can stop here; new readers read 9 bytes (version + size).
    const auto splitVersion =
        storageVersion >= StorageVersionInfo::STORAGE_VERSION_50 || catalogStorageVersion != 0;
    ser.serializeValue(
        static_cast<uint8_t>(splitVersion ? HEADER_FORMAT_VERSION_WITH_CATALOG_VERSION :
                                            HEADER_FORMAT_VERSION_WITH_DATAFILE_NUM_PAGES));
    ser.serializeValue(dataFileNumPages);
    if (splitVersion) {
        ser.serializeValue(getCatalogStorageVersion());
    }
}

DatabaseHeader DatabaseHeader::deserialize(common::Deserializer& deSer) {
    validateMagicBytes(deSer);
    const auto savedStorageVersion = validateStorageVersion(deSer);
    PageRange catalogPageRange{}, metaPageRange{};
    common::uuid databaseID{};
    std::string key;
    deSer.validateDebuggingInfo(key, "catalog");
    deSer.deserializeValue(catalogPageRange.startPageIdx);
    deSer.deserializeValue(catalogPageRange.numPages);
    deSer.validateDebuggingInfo(key, "metadata");
    deSer.deserializeValue(metaPageRange.startPageIdx);
    deSer.deserializeValue(metaPageRange.numPages);
    deSer.validateDebuggingInfo(key, "databaseID");
    deSer.deserializeValue(databaseID.value);
    common::page_idx_t dataFileNumPages = 0;
    uint8_t headerFormatVersion = 0;
    storage_version_t catalogVersion = savedStorageVersion;
    // Old format (e.g. riag-0.14.1) ends here; reading 9 bytes would read beyond logical header.
    // Read one byte: only if it matches our format version do we read the following 8 bytes.
    deSer.deserializeValue(headerFormatVersion);
    if (headerFormatVersion == HEADER_FORMAT_VERSION_WITH_DATAFILE_NUM_PAGES ||
        headerFormatVersion == HEADER_FORMAT_VERSION_WITH_CATALOG_VERSION) {
        deSer.deserializeValue(dataFileNumPages);
    }
    if (headerFormatVersion == HEADER_FORMAT_VERSION_WITH_CATALOG_VERSION) {
        storage_version_t value = 0;
        deSer.deserializeValue(value);
        if (savedStorageVersion >= StorageVersionInfo::STORAGE_VERSION_50) {
            catalogVersion = value;
        }
    }
    if (!StorageVersionInfo::canReadStorageVersion(catalogVersion) ||
        catalogVersion > savedStorageVersion) {
        throw common::RuntimeException("Unsupported catalog payload version in database header.");
    }
    deSer.setStorageVersion(catalogVersion);
    return {catalogPageRange, metaPageRange, dataFileNumPages, databaseID, savedStorageVersion,
        headerFormatVersion == HEADER_FORMAT_VERSION_WITH_CATALOG_VERSION ? catalogVersion : 0};
}

uint64_t DatabaseHeader::storageVersionOffset() {
    auto buffer = std::make_shared<common::BufferWriter>();
    common::Serializer serializer(buffer);
    writeMagicBytes(serializer);
    serializer.writeDebuggingInfo("storage_version");
    return buffer->getSize();
}

uint64_t DatabaseHeader::extensionOffset() {
    auto buffer = std::make_shared<common::BufferWriter>();
    common::Serializer serializer(buffer);
    DatabaseHeader header;
    header.serialize(serializer);
    return buffer->getSize() - sizeof(uint8_t) - sizeof(common::page_idx_t);
}

DatabaseHeader DatabaseHeader::createInitialHeader(common::RandomEngine* randomEngine) {
    // We generate a random UUID to act as the database ID
    return DatabaseHeader{{}, {}, 0, common::UUID::generateRandomUUID(randomEngine),
        StorageVersionInfo::getStorageVersion()};
}

std::optional<DatabaseHeader> DatabaseHeader::readDatabaseHeader(common::FileInfo& dataFileInfo) {
    if (dataFileInfo.getFileSize() < common::RIAG_PAGE_SIZE) {
        // If the data file hasn't been written to there is no existing database header
        return std::nullopt;
    }
    auto reader = std::make_unique<common::BufferedFileReader>(dataFileInfo);
    common::Deserializer deSer(std::move(reader));
    try {
        return DatabaseHeader::deserialize(deSer);
    } catch (const common::RuntimeException&) {
        // It is possible we optimistically write to the database file before the first checkpoint
        // In this case the magic bytes check will fail and we assume there is no existing header
        return std::nullopt;
    }
}
} // namespace riag::storage

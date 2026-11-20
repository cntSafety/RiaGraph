#include "common/file_system/file_system.h"

#include "common/exception/io.h"
#include "common/string_utils.h"
#include <format>

#if defined(_WIN32)
#include "common/windows_utils.h"
#endif

namespace riag {
namespace common {

void FileSystem::overwriteFile(const std::string& /*from*/, const std::string& /*to*/) {
    UNREACHABLE_CODE;
}

void FileSystem::renameFile(const std::string& from, const std::string& to) {
    std::error_code ec;
    std::filesystem::rename(pathFromUTF8(from), pathFromUTF8(to), ec);
    if (ec) {
        throw IOException(
            std::format("Error renaming file {} to {}. ErrorMessage: {}", from, to, ec.message()));
    }
}

void FileSystem::copyFile(const std::string& /*from*/, const std::string& /*to*/) {
    UNREACHABLE_CODE;
}

void FileSystem::createDir(const std::string& /*dir*/) const {
    UNREACHABLE_CODE;
}

void FileSystem::removeFileIfExists(const std::string&, const main::ClientContext* /*context*/) {
    UNREACHABLE_CODE;
}

bool FileSystem::fileOrPathExists(const std::string& /*path*/, main::ClientContext* /*context*/) {
    UNREACHABLE_CODE;
}

std::string FileSystem::expandPath(main::ClientContext* /*context*/,
    const std::string& path) const {
    return path;
}

std::string FileSystem::joinPath(const std::string& base, const std::string& part) {
    return base + "/" + part;
}

std::string FileSystem::getFileExtension(const std::filesystem::path& path) {
    auto extension = path.extension();
    if (isCompressedFile(path)) {
        extension = path.stem().extension();
    }
    return extension.string();
}

bool FileSystem::isCompressedFile(const std::filesystem::path& path) {
    return isGZIPCompressed(path);
}

std::string FileSystem::getFileName(const std::filesystem::path& path) {
    return path.filename().string();
}

std::filesystem::path FileSystem::pathFromUTF8(const std::string& utf8Path) {
#if defined(_WIN32)
    return std::filesystem::path(WindowsUtils::utf8ToUnicode(utf8Path.c_str()));
#else
    return std::filesystem::path(utf8Path);
#endif
}

std::string FileSystem::pathToUTF8(const std::filesystem::path& path) {
#if defined(_WIN32)
    return WindowsUtils::unicodeToUTF8(path.c_str());
#else
    return path.string();
#endif
}

void FileSystem::writeFile(FileInfo& /*fileInfo*/, const uint8_t* /*buffer*/, uint64_t /*numBytes*/,
    uint64_t /*offset*/) const {
    UNREACHABLE_CODE;
}

void FileSystem::truncate(FileInfo& /*fileInfo*/, uint64_t /*size*/) const {
    UNREACHABLE_CODE;
}

void FileSystem::reset(FileInfo& fileInfo) {
    fileInfo.seek(0, SEEK_SET);
}

bool FileSystem::isGZIPCompressed(const std::filesystem::path& path) {
    auto extensionLowerCase = StringUtils::getLower(path.extension().string());
    return extensionLowerCase == ".gz" || extensionLowerCase == ".gzip";
}

} // namespace common
} // namespace riag

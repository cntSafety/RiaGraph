#if defined(_WIN32)
#include "common/windows_utils.h"

#include <memory>
#include <system_error>

#include "common/exception/io.h"
#include <format>

namespace riag {
namespace common {

static void throwUTF8ConversionError() {
    const auto error = GetLastError();
    if (error == ERROR_NO_UNICODE_TRANSLATION) {
        throw IOException("Failure in MultiByteToWideChar: the input is not valid UTF-8.");
    }
    throw IOException(std::format("Failure in MultiByteToWideChar. Error {}: {}", error,
        std::system_category().message(error)));
}

std::wstring WindowsUtils::utf8ToUnicode(const char* input) {
    // Reject invalid UTF-8 instead of substituting U+FFFD: a path that is not UTF-8 would
    // otherwise name a different file than the caller meant.
    uint32_t result;

    result = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, input, -1, nullptr, 0);
    if (result == 0) {
        throwUTF8ConversionError();
    }
    auto buffer = std::make_unique<wchar_t[]>(result);
    result = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, input, -1, buffer.get(), result);
    if (result == 0) {
        throwUTF8ConversionError();
    }
    // The count includes the terminating null character, which is not part of the string.
    return std::wstring(buffer.get(), result - 1);
}

std::string WindowsUtils::unicodeToUTF8(LPCWSTR input) {
    uint64_t resultSize;

    resultSize = WideCharToMultiByte(CP_UTF8, 0, input, -1, 0, 0, 0, 0);
    if (resultSize == 0) {
        throw IOException("Failure in WideCharToMultiByte");
    }
    auto buffer = std::make_unique<char[]>(resultSize);
    resultSize = WideCharToMultiByte(CP_UTF8, 0, input, -1, buffer.get(), resultSize, 0, 0);
    if (resultSize == 0) {
        throw IOException("Failure in WideCharToMultiByte");
    }
    return std::string(buffer.get(), resultSize - 1);
}

} // namespace common
} // namespace riag
#endif

#pragma once

#include <limits>

#include "common/exception/overflow.h"
#include "common/types/string_t.h"
#include "common/vector/value_vector.h"
#include "utf8proc.h"
#include <format>

namespace riag {
namespace function {

// Padding logic has been taken from DuckDB:
// https://github.com/duckdb/duckdb/blob/master/src/function/scalar/string/pad.cpp
struct BasePadOperation {
public:
    static inline void operation(common::string_t& src, int64_t count,
        common::string_t& characterToPad, common::string_t& result,
        common::ValueVector& resultValueVector,
        void (*padOperation)(common::string_t& src, int64_t count, common::string_t& characterToPad,
            std::string& paddedResult)) {
        if (count < 0) {
            count = 0;
        }
        // Every padded character takes at least one byte, and string_t keeps its length in 32
        // bits. The pad helpers below count characters in 32 bits as well, so a larger count
        // must not reach them: it would be truncated, not rejected.
        if (count > static_cast<int64_t>(std::numeric_limits<uint32_t>::max())) {
            throwTooLong();
        }
        std::string paddedResult;
        padOperation(src, count, characterToPad, paddedResult);
        common::StringVector::addString(&resultValueVector, result, paddedResult.data(),
            paddedResult.size());
    }

    [[noreturn]] static void throwTooLong() {
        throw common::OverflowException(
            std::format("The padded string exceeds the maximum string length of {} bytes.",
                std::numeric_limits<uint32_t>::max()));
    }

    static std::pair<uint32_t, uint32_t> padCountChars(const uint32_t count, const char* data,
        const uint32_t size) {
        auto str = reinterpret_cast<const utf8proc::utf8proc_uint8_t*>(data);
        uint32_t byteCount = 0, charCount = 0;
        for (; charCount < count && byteCount < size; charCount++) {
            utf8proc::utf8proc_int32_t codepoint = 0;
            auto bytes = utf8proc::utf8proc_iterate(str + byteCount, size - byteCount, &codepoint);
            byteCount += bytes;
        }
        return {byteCount, charCount};
    }

    static void insertPadding(uint32_t charCount, common::string_t pad, std::string& result) {
        auto padData = pad.getData();
        auto padSize = pad.len;
        uint32_t padByteCount = 0;
        for (auto i = 0u; i < charCount; i++) {
            if (padByteCount >= padSize) {
                if (result.size() + padByteCount > std::numeric_limits<uint32_t>::max()) {
                    throwTooLong();
                }
                result.insert(result.end(), (char*)padData, (char*)(padData + padByteCount));
                padByteCount = 0;
            }
            utf8proc::utf8proc_int32_t codepoint = 0;
            auto bytes = utf8proc::utf8proc_iterate(padData + padByteCount, padSize - padByteCount,
                &codepoint);
            padByteCount += bytes;
        }
        if (result.size() + padByteCount > std::numeric_limits<uint32_t>::max()) {
            throwTooLong();
        }
        result.insert(result.end(), (char*)padData, (char*)(padData + padByteCount));
    }
};

} // namespace function
} // namespace riag

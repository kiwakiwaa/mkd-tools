//
// kiwakiwaaにより 2026/01/16 に作成されました。
//

#pragma once

#include "MKD/result.hpp"
#include <zlib.h>

#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <vector>

namespace MKD
{
    class ZlibStream
    {
    public:

        ZlibStream() = default;
        ~ZlibStream() = default;

        // not copyable
        ZlibStream(const ZlibStream&) = delete;
        ZlibStream& operator=(const ZlibStream&) = delete;

        // movable
        ZlibStream(ZlibStream&&) = default;
        ZlibStream& operator=(ZlibStream&&) = default;


        /**
         * Compresses data using zlib compression
         * @param data Span of uncompressed input data
         * @param level Compression level (0-9, where 0 = no compression, 1 = fastest, 9 = best compression)
         * @return Span pointing to compressed data in internal buffer or error string
         */
        [[nodiscard]] Result<std::span<const uint8_t>> compress(std::span<const uint8_t> data, int level = Z_DEFAULT_COMPRESSION) const;

        /**
         * Decompresses zlib-compressed data into the internal buffer
         * @param compressed Span of compressed input data
         * @param expectedSize Initial guess for decompressed size
         * @return Span pointing to decompressed data in internal buffer or error string
         */
        [[nodiscard]] Result<std::span<const uint8_t>> decompress(std::span<const uint8_t> compressed, size_t expectedSize) const;

        /**
         * Takes ownership of the internal decompression buffer
         * @return Vector containing the decompressed data
         */
        [[nodiscard]] std::vector<uint8_t> takeBuffer() const;

        /**
         * Detects if data is zlib-compressed by examining header bytes
         * @param data Span of data to check
         * @return true if data appears to be zlib-compressed
         */
        static bool isZlibCompressed(std::span<const uint8_t> data);

    private:
        mutable std::vector<uint8_t> buffer_;
    };
}

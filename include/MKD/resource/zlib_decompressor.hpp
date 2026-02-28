//
// kiwakiwaaにより 2026/01/16 に作成されました。
//

#pragma once

#include "MKD/result.hpp"

#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <vector>


namespace MKD
{
    class ZlibDecompressor
    {
    public:

        ZlibDecompressor() = default;
        ~ZlibDecompressor() = default;

        // not copyable
        ZlibDecompressor(const ZlibDecompressor&) = delete;
        ZlibDecompressor& operator=(const ZlibDecompressor&) = delete;

        // movable
        ZlibDecompressor(ZlibDecompressor&&) = default;
        ZlibDecompressor& operator=(ZlibDecompressor&&) = default;

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
        mutable std::vector<uint8_t> decompressBuffer_;
    };
}

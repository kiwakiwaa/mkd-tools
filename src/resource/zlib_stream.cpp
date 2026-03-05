//
// kiwakiwaaにより 2026/01/16 に作成されました。
//

#include "zlib_stream.hpp"

#include <iostream>
#include <format>

namespace MKD
{
    Result<std::span<const uint8_t>> ZlibStream::compress(std::span<const uint8_t> data, int level) const
    {
        if (level < Z_DEFAULT_COMPRESSION || level > 9)
            return std::unexpected(std::format("Invalid compression level: {}. Must be between -1 and 9", level));

        // ensures we have enough space
        const uLong boundSize = compressBound(data.size());
        buffer_.resize(boundSize);

        z_stream stream{};
        stream.next_in = const_cast<Bytef*>(data.data());
        stream.avail_in = static_cast<uInt>(data.size());
        stream.next_out = buffer_.data();
        stream.avail_out = static_cast<uInt>(boundSize);

        if (int initResult = deflateInit(&stream, level); initResult != Z_OK)
            return std::unexpected(std::format("Failed to initialise zlib compression with level {}: error code {}", level, initResult));

        if (int result = deflate(&stream, Z_FINISH); result != Z_STREAM_END) {
            deflateEnd(&stream);
            if (result == Z_OK)
                return std::unexpected("Compression buffer too small, this shouldn't happen with compressBound");

            return std::unexpected(std::format("Zlib compression failed with error code: {}", result));
        }

        const size_t compressedSize = stream.total_out;
        deflateEnd(&stream);

        buffer_.resize(compressedSize);

        return std::span<const uint8_t>(buffer_.data(), compressedSize);
    }


    Result<std::span<const uint8_t>> ZlibStream::decompress(std::span<const uint8_t> compressed, const size_t expectedSize) const
    {
        buffer_.resize(expectedSize);

        z_stream stream{};
        stream.next_in = const_cast<Bytef*>(compressed.data());
        stream.avail_in = static_cast<uInt>(compressed.size());
        stream.next_out = buffer_.data();
        stream.avail_out = static_cast<uInt>(expectedSize);

        if (inflateInit(&stream) != Z_OK)
            return std::unexpected("Failed to initialise zlib decompression");

        int result;
        do
        {
            result = inflate(&stream, Z_NO_FLUSH);

            if (result == Z_BUF_ERROR || (result == Z_OK && stream.avail_out == 0))
            {
                // Need more buffer space
                const size_t currentSize = buffer_.size();
                buffer_.resize(currentSize * 2);
                stream.next_out = buffer_.data() + currentSize;
                stream.avail_out = static_cast<uInt>(currentSize);
            }
            else if (result != Z_OK && result != Z_STREAM_END)
            {
                inflateEnd(&stream);
                return std::unexpected(std::format("Zlib decompression failed with error code: {}", result));
            }

        } while (result == Z_OK);

        const size_t decompressedSize = stream.total_out;
        inflateEnd(&stream);

        return std::span<const uint8_t>(buffer_.data(), decompressedSize);
    }


    std::vector<uint8_t> ZlibStream::takeBuffer() const
    {
        return std::move(buffer_);
    }


    bool ZlibStream::isZlibCompressed(const std::span<const uint8_t> data)
    {
        return data.size() > 2 &&
               data[0] == 0x78 &&
               (data[1] == 0x01 || data[1] == 0x5E || data[1] == 0x9C || data[1] == 0xDA);
    }
}
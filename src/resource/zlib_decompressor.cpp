//
// kiwakiwaaにより 2026/01/16 に作成されました。
//

#include "monokakido/resource/zlib_decompressor.hpp"

#include <iostream>
#include <format>
#include <zlib.h>


namespace monokakido::resource
{

    std::expected<std::span<const uint8_t>, std::string> ZlibDecompressor::decompress(std::span<const uint8_t> compressed, const size_t expectedSize) const
    {
        decompressBuffer_.resize(expectedSize);

        z_stream stream{};
        stream.next_in = const_cast<Bytef*>(compressed.data());
        stream.avail_in = static_cast<uInt>(compressed.size());
        stream.next_out = decompressBuffer_.data();
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
                const size_t currentSize = decompressBuffer_.size();
                decompressBuffer_.resize(currentSize * 2);
                stream.next_out = decompressBuffer_.data() + currentSize;
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

        return std::span<const uint8_t>(decompressBuffer_.data(), decompressedSize);
    }


    std::vector<uint8_t> ZlibDecompressor::takeBuffer() const
    {
        return std::move(decompressBuffer_);
    }



    bool ZlibDecompressor::isZlibCompressed(std::span<const uint8_t> data)
    {
        return data.size() > 2 &&
               data[0] == 0x78 &&
               (data[1] == 0x01 || data[1] == 0x5E || data[1] == 0x9C || data[1] == 0xDA);
    }


}
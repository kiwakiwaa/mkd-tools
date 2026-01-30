//
// kiwakiwaaにより 2026/01/30 に作成されました。
//

#include "monokakido/resource/rsc/rsc_decryptor.hpp"


namespace monokakido::resource
{
    std::expected<std::vector<uint8_t>, std::string> RscDecryptor::decrypt(
        const std::span<const uint8_t> encryptedData, const std::array<uint8_t, 32>& key)
    {
        // Validate input
        if (encryptedData.size() < 4)
        {
            return std::unexpected("Encrypted data too short (minimum 4 bytes)");
        }

        // Extract checksum (last 4 bytes)
        const size_t dataLength = encryptedData.size() - 4;
        uint32_t checksum;
        std::memcpy(&checksum, encryptedData.data() + dataLength, sizeof(uint32_t));

        // XOR checksum with constant to get actual output length
        const uint32_t outputLength = checksum ^ CHECKSUM_XOR;

        // validate output length
        if (outputLength > dataLength || outputLength > 100'000'000)
        {
            return std::unexpected("Invalid output length derived from checksum");
        }

        std::vector<uint8_t> output(dataLength);

        // Permute data using DATA2 lookup table
        if (dataLength > 0)
        {
            // calculate starting index
            uint32_t tableIndex = calculateTableIndex(checksum ^ CHECKSUM_XOR);

            const uint8_t* src = encryptedData.data();
            const uint8_t* srcEnd = src + dataLength;
            uint8_t* dst = output.data();

            // process in 16 byte blocks
            while (src < srcEnd)
            {
                const size_t blockSize = std::min<size_t>(16, srcEnd - src);
                const auto& permutation = DATA2[tableIndex];

                // apply permutation
                for (size_t i = 0; i < blockSize; ++i)
                {
                    dst[permutation[i]] = src[i];
                }

                // move to next block
                src += blockSize;
                dst += blockSize;

                // wrap table index
                tableIndex = (tableIndex + 1) % 31;
            }
        }

        // XOR with key and DATA1
        if (dataLength > 0)
        {
            const uint32_t data1Start = (checksum ^ CHECKSUM_XOR) & 0x1F;

            size_t data1Pos = data1Start;
            size_t keyPos = 0;

            for (size_t i = 0; i < dataLength; ++i)
            {
                // XOR with key and DATA1
                output[i] ^= key[keyPos] ^ DATA1[data1Pos];

                // advance positions
                data1Pos = (data1Pos + 1) % 32;
                keyPos = (keyPos + 1) % 32;

                // reset both when data1 wraps
                if (data1Pos == 0)
                {
                    keyPos = 0;
                }
            }
        }

        output.resize(outputLength);
        return output;
    }


    uint32_t RscDecryptor::calculateTableIndex(const uint32_t value)
    {
        // Original assembly logic:
        // edx * 0x8421085 >> 32, then some shifts and additions
        const uint64_t mul = static_cast<uint64_t>(value) * 0x8421085ULL;
        const uint32_t esi = static_cast<uint32_t>(mul >> 32);

        uint32_t edx = value - esi;
        edx = edx >> 1;
        edx = edx + esi;
        edx = edx >> 4;
        uint32_t result = edx * 31;
        result = value - result;
        return result % 31;
    }
}

//
// kiwakiwaaにより 2026/01/30 に作成されました。
//

#pragma once

#include <array>
#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace monokakido
{
    /**
     * Decrypts .rsc file chunks from dictionaries
     * - This is a custom cipher, not a standard cryptographic algorithm
     * - The DATA1 and DATA2 tables are embedded in the app binary
     * - Each dictionary product uses a unique 32-byte key
     */
    class RscCrypto
    {
    public:

        /**
         * Derives the 32-byte decryption key for encrypted RSC resources.
         *
         * 1. Compute SHA-256 of the dictionary identifier string
         * 2. XOR the hash with a rolling window into a fixed 31-byte substitution table,
         *    where the starting offset is determined by byte 7 of the hash (mod 31)
         *
         * @param dictId Dictionary identifier (e.g. "DAIJISEN2.J")
         * @return 32-byte derived key
         */
        static std::array<uint8_t, 32> deriveKey(std::string_view dictId);

        /**
         * Decrypt RSC data using HMDicEncoder::Decode algorithm
         *
         * @param encryptedData Input data (includes 4-byte checksum at end)
         * @param key 32-byte decryption key
         * @return Decrypted data on success, or error message on failure
         *
         * @note The key must match the one used during encryption, otherwise
         *       the output will be garbage data.
         */
        static std::expected<std::vector<uint8_t>, std::string> decrypt(std::span<const uint8_t> encryptedData,
                                                                        const std::array<uint8_t, 32>& key);

    private:

        /**
         * Reverse the block permutation applied during encryption
         *
         * Processes data in 16-byte blocks, reordering bytes within each block
         * according to permutation patterns in DATA2. The pattern rotates
         * every block based on the checksum value.
         *
         * @param src Source encrypted data (without checksum)
         * @param dst Destination buffer (same size as src)
         * @param checksum Checksum value from encrypted data
         */
        static void permuteData(std::span<const uint8_t> src, std::span<uint8_t> dst, uint32_t checksum);

        /**
         * Apply XOR cipher using dictionary key and static table
         *
         * Each byte is XORed with corresponding positions from both the
         * dictionary-specific key and the static DATA1 table.
         *
         * @param data Data to decrypt
         * @param key 32-byte dictionary-specific key
         * @param checksum Checksum value (determines starting position)
         */
        static void applyXorCipher(std::span<uint8_t> data, const std::array<uint8_t, 32>& key, uint32_t checksum);

        /**
         * Cross platform SHA256 helper
         *
         * @param data data to hash
         * @return 32 byte hash
         */
        static std::array<uint8_t, 32> sha256(std::span<const uint8_t> data);

    };
}

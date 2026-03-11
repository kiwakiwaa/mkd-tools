//
// kiwakiwaaにより 2026/01/22 に作成されました。
//

#pragma once

#include "MKD/result.hpp"
#include "MKD/resource/retained_span.hpp"
#include "platform/read_sequence.hpp"
#include "resource_store_index.hpp"
#include "resource_store_crypto.hpp"

#include <array>
#include <filesystem>
#include <memory>
#include <optional>
#include <vector>

#if defined(__APPLE__) || defined(__linux__)
#include "../../platform/mmap_file.hpp"
#include <mutex>
#include <unordered_map>
#endif


namespace fs = std::filesystem;

namespace MKD
{
    /**
    * Resource Store Contents file format (.rsc)
    *
    * The .rsc format is the content storage system for XML entry data, audio and fonts.
    * .rsc files contain compressed (and also optionally encrypted) dictionary entries in XML format.
    *  - also used in `/index` for dict RUIGO (Apple binary .plist files - not implemented yet)
    *
    * Overview:
    * ┌─────────────────────────────────────────────────────────────────┐
    * │ Index Layer (contents.idx + contents.map)                       │
    * │  ├─ contents.idx: Maps item IDs to map indices                  │
    * │  └─ contents.map: Maps indices to chunk locations               │
    * ├─────────────────────────────────────────────────────────────────┤
    * │ Data Layer (0.rsc, 1.rsc, 2.rsc, ...)                           │
    * │  ├─ Multiple numbered files for size management                 │
    * │  ├─ Each file contains multiple compressed "chunks"             │
    * │  └─ Each chunk contains multiple dictionary entries             │
    * └─────────────────────────────────────────────────────────────────┘
    *
    * CHUNKS: Compressed blocks containing multiple dictionary entries
    * - Each chunk can be zlib-compressed (optionally encrypted+compressed)
    * - Chunks are variable-sized
    * - Multiple entries sharing a chunk reduces decompression overhead
    *
    * GLOBAL OFFSETS: Unified addressing across all .rsc files
    * - Treats all numbered files (0.rsc, 1.rsc, ...) as one virtual file
    * - Offsets in MapRecords are relative to the start of 0.rsc
    * - Example: offset 5MB in a 3MB+4MB file setup points to byte 2MB of 1.rsc
    *
    * Chunk Structure (per chunk in .rsc files):
    *
    * Two formats exist, distinguished by the first 4 bytes:
    *
    * OLD FORMAT (unencrypted):
    * ┌──────────────────────────────────────┐
    * │ Compressed Length    (4 bytes, > 0)  │
    * ├──────────────────────────────────────┤
    * │ Zlib Data            (variable)      │
    * └──────────────────────────────────────┘
    *
    * NEW FORMAT (encrypted):
    * ┌──────────────────────────────────────┐
    * │ Zero Marker          (4 bytes, = 0)  │
    * ├──────────────────────────────────────┤
    * │ Encrypted Length     (4 bytes)       │
    * ├──────────────────────────────────────┤
    * │ Encrypted Data       (variable)      │
    * └──────────────────────────────────────┘
    *
    * Encryption Details:
    * - Algorithm: XOR based algorithm 'HMDicEncoder::Decode'
    * - Key: Dictionary-specific 32-byte key
    * - Format marker: First 4 bytes are 0x00000000
    * - Process: Decrypt → Decompress → Parse items
    */

#if defined(__APPLE__) || defined(__linux__)
    struct ResourceStoreContentsFile
    {
        size_t sequenceNumber;
        size_t globalOffset;
        size_t length;
        fs::path filePath;
        MappedFile mapping;
    };
#else
    struct ResourceStoreContentsFile
    {
        size_t sequenceNumber;
        size_t globalOffset;
        size_t length;
        fs::path filePath;
    };
#endif


    class ResourceStoreContents
    {
    public:
        /**
         * Initialise RscData form a dictionary containing .rsc files
         * @param directoryPath Directory containing 0.rsc, 1.rsc, etc.
         * @param dictId Dictionary id for decryption key lookup
         * @param mapVersion Version of the .map file (to determine whether key should be derived or not)
         * @return RscData instance or error string if failure
         */
        static Result<ResourceStoreContents> load(const fs::path& directoryPath, std::string_view dictId = "",
                                                  uint32_t mapVersion = 0);

        /**
         * Retrieve the data for a dictionary entry
         *
         * Given a MapRecord (obtained from RscIndex), this function:
         * 1. Loads and decompresses the chunk  if not already cached
         * 2. Parses the specific item from within the decompressed chunk
         * 3. Returns a span view of the item's content
         *
         * @param record MapRecord specifying the chunk offset and item offset
         * @return Span view of item data, or error string if failure
         */
        [[nodiscard]] Result<RetainedSpan> get(const ResourceStoreMapRecord& record) const;

    private:
        /**
         * Private constructor - use load() to create instances
         */
        explicit ResourceStoreContents(std::vector<ResourceStoreContentsFile>&& files,
                                       const std::optional<std::array<uint8_t, 32>>& decryptionKey);

        /**
         * Discover all .rsc files in a directory
         *
         * @param directoryPath Directory to scan
         * @return Vector of discovered files or error string
         */
        static Result<std::vector<ResourceStoreContentsFile>> discoverFiles(const fs::path& directoryPath);

        /**
         * Reads data directly at global offset. Typically used for fonts
         *
         * @param globalOffset Offset in global address space
         * @return Span view to the data
         */
        [[nodiscard]] Result<RetainedSpan> readDirectData(size_t globalOffset) const;

        /**
         * Parse a single item from the currently loaded chunk
         *
         * @param offset Offset within chunkBuffer_ where item begins
         * @return Span view of item content or error string
         */
        static Result<RetainedSpan> parseItemFromChunk(std::shared_ptr<const std::vector<uint8_t>> chunk, size_t offset);

        std::vector<ResourceStoreContentsFile> files_;
        std::optional<std::array<uint8_t, 32>> decryptionKey_;

#if defined(__APPLE__) || defined(__linux__)
        /**
         * Resolves a global offset to a direct memory span
         * @param offset Offset in global address space
         * @param length Number of bytes to access from the offset
         * @return Span of bytes directly mapped from the appropriate .rsc file or error string
         */
        [[nodiscard]] Result<std::span<const uint8_t>> resolveGlobal(size_t offset, size_t length) const;

        /**
         * Retrieves or loads a compressed chunk
         * @param globalOffset Global offset pointing to the start of a chunk
         * @return Shared pointer to the decoded chunk data or error string
         */
        [[nodiscard]] Result<std::shared_ptr<const std::vector<uint8_t>>> getOrLoadChunk(size_t globalOffset) const;

        /**
         * Decodes a chunk at the specified global offset
         * @param globalOffset Offset in global address space
         * @return Decompressed chunk data as vector of bytes or error string
         */
        [[nodiscard]] Result<std::vector<uint8_t>> decodeChunkAt(size_t globalOffset) const;

        /**
         * Reads and decrypts an encrypted region
         * @param globalOffset Offset in global address space
         * @return Decrypted data as vector of bytes or error string
         */
        [[nodiscard]] Result<std::vector<uint8_t>> readEncryptedRegion(size_t globalOffset) const;

        struct ChunkCache
        {
            std::mutex mutex;
            std::unordered_map<size_t, std::shared_ptr<const std::vector<uint8_t>>> entries;
        };

        std::unique_ptr<ChunkCache> cache_;
        static constexpr size_t MAX_CACHED_CHUNKS = 64;
#else
        /**
         * Load and decompress a chunk from disk
         *
         * @param globalOffset Global offset to the start of the chunk
         * @return void on success, error string on failure
         */
        Result<std::shared_ptr<const std::vector<uint8_t>>> loadChunk(size_t globalOffset) const;

        /**
         * Find which .rsc file contains a given global offset
         *
         * @param globalOffset Offset in global address space
         * @return (file reference, local offset) pair or error string
         */
        Result<std::pair<const ResourceStoreContentsFile&, size_t>> findFileByOffset(size_t globalOffset) const;

        /**
         * Open and seek a reader to the local position for a global offset
         * @param globalOffset Offset in global address space
         * @return BinaryFileReader or error string
         */
        Result<BinaryFileReader> openReaderAt(size_t globalOffset) const;

        /**
         * Reads and processes the data chunk using the provided BinaryFile reader
         * - reads the format marker and handles decryption and decompression as needed
         *
         * @param reader Binary file reader for reading the chunk
         * @return Decoded data, or error string if failure
         */
        Result<std::vector<uint8_t>> readAndProcessChunk(BinaryFileReader& reader) const;

        /**
         * Helper function to read and decrypt chunk data from the new format
         * - uses ResourceStoreCrypto::decrypt to decrypt the data
         *
         * @param reader Binary file reader for reading the chunk
         * @return Decrypted data, or error string if failure
         */
        Result<std::vector<uint8_t>> readAndDecryptData(BinaryFileReader& reader) const;

        mutable std::shared_ptr<const std::vector<uint8_t>> currentChunk_;
        mutable size_t currentChunkOffset_ = SIZE_MAX;
#endif
    };
}

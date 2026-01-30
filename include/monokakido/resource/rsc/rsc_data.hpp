//
// kiwakiwaaにより 2026/01/22 に作成されました。
//

#pragma once

#include "monokakido/resource/rsc/rsc_index.hpp"
#include "monokakido/resource/zlib_decompressor.hpp"

#include <array>
#include <filesystem>
#include <memory>
#include <optional>
#include <vector>


namespace fs = std::filesystem;

namespace monokakido::resource
{
    /**
    * RSC Resource File Format (.rsc)
    * ================================
    *
    * The .rsc format is Monokakido's primary dictionary content storage system.
    * .rsc files contain compressed (and also optionally encrypted) dictionary entries in XML format.
    *
    * Overview:
    * ┌─────────────────────────────────────────────────────────────────┐
    * │ Index Layer (contents.idx + contents.map)                       │
    * │  ├─ contents.idx: Maps item IDs to map indices                  │
    * │  └─ contents.map: Maps indices to chunk locations               │
    * ├─────────────────────────────────────────────────────────────────┤
    * │ Data Layer (0.rsc, 1.rsc, 2.rsc, ...)                          │
    * │  ├─ Multiple numbered files for size management                 │
    * │  ├─ Each file contains multiple compressed "chunks"             │
    * │  └─ Each chunk contains multiple dictionary entries             │
    * └─────────────────────────────────────────────────────────────────┘
    *
    * CHUNKS: Compressed blocks containing multiple dictionary entries
    * - Each chunk is zlib-compressed (or optionally encrypted+compressed)
    * - Chunks are variable-sized
    * - Multiple entries sharing a chunk reduces decompression overhead
    *
    * GLOBAL OFFSETS: Unified addressing across all .rsc files
    * - Treats all numbered files (0.rsc, 1.rsc, ...) as one virtual file
    * - Offsets in MapRecords are relative to the start of 0.rsc
    * - Example: offset 5MB in a 3MB+4MB file setup points to byte 2MB of 1.rsc
    *
    * Chunk Structure (per chunk in .rsc files):
    * ┌─────────────────────────────────────────────────────────────────┐
    * │ Format Marker (4 bytes)                                         │
    * │  - 0x00000000: New encrypted format                             │
    * │  - Other values: Compressed data length (old format)            │
    * ├─────────────────────────────────────────────────────────────────┤
    * │ Compressed/Encrypted Data (variable length)                     │
    * │  - Old format: Direct zlib compression                          │
    * │  - New format: encrypted, then zlib compressed                  │
    * └─────────────────────────────────────────────────────────────────┘
    *
    * Decompressed Chunk Contents:
    * ┌─────────────────────────────────────────────────────────────────┐
    * │ Item 1                                                          │
    * │  ├─ Header (4 or 8 bytes)                                       │
    * │  │   - Old format: [4-byte length]                              │
    * │  │   - New format: [4-byte unknown][4-byte length]              │
    * │  └─ Content (XML data, for dictionaries)                        │
    * ├─────────────────────────────────────────────────────────────────┤
    * │ Item 2                                                          │
    * │  ├─ Header                                                      │
    * │  └─ Content                                                     │
    * ├─────────────────────────────────────────────────────────────────┤
    * │ ... (more items)                                                │
    * └─────────────────────────────────────────────────────────────────┘
    *
    * Encryption Details:
    * - Algorithm: Custom XOR based algorithm 'HMDicEncoder::Decode'
    * - Key: Dictionary-specific 32-byte key
    * - Format marker: First 4 bytes are 0x00000000
    * - Process: Decrypt → Decompress → Parse items
    */

    struct RscResourceFile
    {
        uint32_t sequenceNumber;
        size_t globalOffset;
        size_t length;
        fs::path filePath;
    };


    class RscData
    {
    public:
        /**
         * Initialise RscData form a dictionary containing .rsc files
         * @param directoryPath Directory containing 0.rsc, 1.rsc, etc.
         * @param dictId Dictionary id for decryption key lookup
         * @return RscData instance or error string if failure
         */
        static std::expected<RscData, std::string> load(const fs::path& directoryPath, std::string_view dictId);

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
         *
         * @warning The returned span is only valid until the next call to get()
         */
        [[nodiscard]] std::expected<std::span<const uint8_t>, std::string> get(const MapRecord& record);

    private:
        /**
         * Private constructor - use load() to create instances
         */
        explicit RscData(std::vector<RscResourceFile>&& files, std::optional<std::array<uint8_t, 32>>&& decryptionKey);

        /**
         * Discover all .rsc files in a directory
         *
         * @param directoryPath Directory to scan
         * @return Vector of discovered files or error string
         */
        static std::expected<std::vector<RscResourceFile>, std::string> discoverFiles(const fs::path& directoryPath);

        /**
         * Find which .rsc file contains a given global offset
         *
         * @param globalOffset Offset in global address space
         * @return (file reference, local offset) pair or error string
         */
        std::expected<std::pair<RscResourceFile&, size_t>, std::string> findFileByOffset(size_t globalOffset);

        /**
         * Load and decompress a chunk from disk
         *
         * @param globalOffset Global offset to the start of the chunk
         * @return void on success, error string on failure
         */
        std::expected<void, std::string> loadChunk(size_t globalOffset);

        /**
         * Parse a single item from the currently loaded chunk
         *
         * @param offset Offset within chunkBuffer_ where item begins
         * @return Span view of item content or error string
         */
        std::expected<std::span<const uint8_t>, std::string> parseItemFromChunk(size_t offset) const;

        std::vector<RscResourceFile> files_;
        std::optional<std::array<uint8_t, 32>> decryptionKey_;
        std::unique_ptr<ZlibDecompressor> decompressor_;

        mutable std::vector<uint8_t> chunkBuffer_;
        mutable size_t currentChunkOffset_ = SIZE_MAX;
    };
}
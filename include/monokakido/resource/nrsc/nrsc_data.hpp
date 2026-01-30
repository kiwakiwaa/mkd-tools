//
// kiwakiwaaにより 2026/01/16 に作成されました。
//

#pragma once

#include "nrsc_index.hpp"
#include "monokakido/resource/zlib_decompressor.hpp"

#include <expected>
#include <fstream>
#include <filesystem>
#include <span>
#include <vector>

namespace fs = std::filesystem;

namespace monokakido::resource
{
    /**
    * NRSC Resource File Format (.nrsc)
    * ==================================
    *
    * Resource files store the actual data (images, audio, etc.) referenced by
    * the index. Multiple numbered files (0.nrsc, 1.nrsc, 2.nrsc, ...) are used
    * to split large resource collections into chunks.
    *
    * Workflow
    * - Load NrscIndex to get the table of contents
    * - Load NrscData to access the numbered resource files
    * - Look up a resource ID in the index to get an NrscIndexRecord
    * - Pass the record to NrscData::get() to retrieve the data
    *
    * Example
    * auto index = NrscIndex::load("dict/graphics");
    * auto data = NrscData::load("dict/graphics");
    *
    * auto record = index->findById("icon_search.png");
    * auto bytes = data->get(*record);  // Retrieves and decompresses if needed
    */

    // Represents a single .nrsc data file
    struct ResourceFile
    {
        uint32_t sequenceNumber;    // Which numbered .nrsc file (0.nrsc, 1.nrsc, etc.)
        fs::path filePath;          // Path to .nrsc file
    };


    class NrscData
    {
    public:
        /**
         * Inits NrscData from a directory containing the .nrsc files
         *
         * @param directoryPath Path to directory
         * @return NrscData class or string if failure
         */
        static std::expected<NrscData, std::string> load(const fs::path& directoryPath);

        /**
         * Gets span view of the data for a given index record
         * Data is decompressed automatically if needed
         *
         * @param record
         * @return Span view of the data, or error string if failure
         * @warning The returned span is only valid until the next call to get()
         */
        [[nodiscard]] std::expected<std::span<const uint8_t>, std::string> get(const NrscIndexRecord& record) const;


    private:

        explicit NrscData(std::vector<ResourceFile>&& files);

        /**
         * Reads raw bytes from the .nrsc file into readBuffer_
         *
         * @param file ResourceFile containing the path and global offset information
         * @param record NrscIndexRecord specifying the global offset and length to read
         * @return void on success, or error string if failure
         */
        [[nodiscard]] std::expected<void, std::string> readFromFile(const ResourceFile& file, const NrscIndexRecord& record) const;


        /**
         * Decompresses data from readBuffer_ based on compression format
         *
         * @param record rscIndexRecord specifying compression format and expected decompressed length
         * @return Span view of the decompressed data, or error string if failure
         * @warning The returned span is only valid until the next call to get(), as it may
         *          reference readBuffer_ or the decompressor's internal buffer
         */
        [[nodiscard]] std::expected<std::span<const uint8_t>, std::string> decompressData(const NrscIndexRecord& record) const;

        std::vector<ResourceFile> files_;
        std::unique_ptr<ZlibDecompressor> decompressor_;
        mutable std::vector<uint8_t> readBuffer_;

    };

}
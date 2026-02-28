//
// kiwakiwaaにより 2026/01/16 に作成されました。
//

#pragma once

#include "MKD/result.hpp"
#include "MKD/resource/owned_span.hpp"
#include "MKD/resource/nrsc/nrsc_index.hpp"
#include "MKD/resource/zlib_decompressor.hpp"

#include <expected>
#include <fstream>
#include <filesystem>
#include <vector>

#if defined(__APPLE__) || defined(__linux__)
    #include "MKD/platform/mmap_file.hpp"
#endif

namespace fs = std::filesystem;

namespace MKD
{
    /**
    * NRSC Resource File Format (.nrsc)
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

#if defined(__APPLE__) || defined(__linux__)
    struct NrscResourceFile
    {
        uint32_t sequenceNumber; // Which numbered .nrsc file (0.nrsc, 1.nrsc, etc.)
        fs::path filePath;       // Path to .nrsc file
        MappedFile mapping;
    };
#else
    struct NrscResourceFile
    {
        uint32_t sequenceNumber;
        fs::path filePath;
    };
#endif

    class NrscData
    {
    public:
        /**
         * Inits NrscData from a directory containing the .nrsc files
         *
         * @param directoryPath Path to directory
         * @return NrscData class or string if failure
         */
        static Result<NrscData> load(const fs::path& directoryPath);

        /**
         * Gets span view of the data for a given index record
         * Data is decompressed automatically if needed
         *
         * @param record
         * @return Span view of the data, or error string if failure
         * @warning The returned span is only valid until the next call to get()
         */
        [[nodiscard]] Result<OwnedSpan> get(const NrscIndexRecord& record) const;


    private:
        explicit NrscData(std::vector<NrscResourceFile>&& files);

        static Result<std::vector<NrscResourceFile>> discoverFiles(const fs::path& directoryPath);

        /**
         * Reads raw bytes from the .nrsc file into readBuffer_
         *
         * @param file ResourceFile containing the path and global offset information
         * @param record NrscIndexRecord specifying the global offset and length to read
         * @return void on success, or error string if failure
         */
        static Result<OwnedSpan> readUncompressed(const NrscResourceFile& file, const NrscIndexRecord& record);


        /**
         * Decompresses data from readBuffer_ based on compression format
         *
         * @param record rscIndexRecord specifying compression format and expected decompressed length
         * @return Span view of the decompressed data, or error string if failure
         * @warning The returned span is only valid until the next call to get(), as it may
         *          reference readBuffer_ or the decompressor's internal buffer
         */
        static Result<OwnedSpan> readCompressed(const NrscResourceFile& file, const NrscIndexRecord& record);

        std::vector<NrscResourceFile> files_;
    };

}
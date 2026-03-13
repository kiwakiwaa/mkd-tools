//
// kiwakiwaaにより 2026/01/16 に作成されました。
//

#pragma once

#include "MKD/result.hpp"
#include "MKD/resource/retained_span.hpp"
#include "../../platform/mmap_file.hpp"
#include "named_resource_store_index.hpp"

#include <expected>
#include <filesystem>
#include <vector>

namespace fs = std::filesystem;

namespace MKD
{
    /**
    * Named Resource Store Contents file format (.nrsc)
    *
    * Resource files store the actual data (images, audio, etc.) referenced by
    * the index. Multiple numbered files (0.nrsc, 1.nrsc, 2.nrsc, ...) are used
    * to split large resource collections into chunks.
    *
    * Example
    * auto index = NrscIndex::load("dict/graphics");
    * auto data = NrscData::load("dict/graphics");
    *
    * auto record = index->findById("icon_search.png");
    * auto bytes = data->get(*record);  // Retrieves and decompresses if needed
    */
    struct NamedResourceStoreFile
    {
        uint32_t sequenceNumber; // Which numbered .nrsc file (0.nrsc, 1.nrsc, etc.)
        fs::path filePath;       // Path to .nrsc file
        MappedFile mapping;
    };

    class NamedResourceStoreContents
    {
    public:
        /**
         * Inits NrscData from a directory containing the .nrsc files
         *
         * @param directoryPath Path to directory
         * @return NrscData class or string if failure
         */
        static Result<NamedResourceStoreContents> load(const fs::path& directoryPath);

        /**
         * Gets span view of the data for a given index record
         * Data is decompressed automatically if needed
         *
         * @param record
         * @return Span view of the data, or error string if failure
         */
        [[nodiscard]] Result<RetainedSpan> get(const NamedResourceStoreIndexRecord& record) const;


    private:
        explicit NamedResourceStoreContents(std::vector<NamedResourceStoreFile>&& files);

        /**
         * Collects all .nrsc files in the directory and sorts them by sequence number
         * - Memory maps the files for direct read access
         *
         * @param directoryPath Path to directory
         * @return Vector of Nrsc resource files
         */
        static Result<std::vector<NamedResourceStoreFile>> discoverFiles(const fs::path& directoryPath);

        /**
         * Reads raw bytes from the .nrsc file mapping
         *
         * @param file ResourceFile containing the path and mapped data
         * @param record NrscIndexRecord specifying the global offset and length to read
         * @return Span view on success, or error string if failure
         */
        static Result<RetainedSpan> readUncompressed(const NamedResourceStoreFile& file, const NamedResourceStoreIndexRecord& record);

        /**
         * Decompresses data from the mapped file based on compression format
         *
         * @param record rscIndexRecord specifying compression format and expected decompressed length
         * @return Span view of the decompressed data, or error string if failure
         */
        static Result<RetainedSpan> readCompressed(const NamedResourceStoreFile& file, const NamedResourceStoreIndexRecord& record);

        std::vector<NamedResourceStoreFile> files_;
    };

}

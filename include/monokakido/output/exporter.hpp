//
// kiwakiwaaにより 2026/01/17 に作成されました。
//

#pragma once

#include "export_options.hpp"

#include <expected>
#include <functional>


namespace monokakido::resource
{
    struct ExportResult
    {
        size_t totalResources;
        size_t exported;
        size_t skipped;
        size_t failed;
        uint64_t totalBytes;
        std::vector<std::string> errors; // List of errors encountered
    };


    template<typename Collection>
    concept ResourceCollection = requires(Collection c, size_t i)
    {
        { c.size() } -> std::convertible_to<size_t>;
        { c.getByIndex(i) } -> std::same_as<std::expected<ResourceItem, std::string>>;
        { c.begin() } -> std::forward_iterator;
        { c.end() } -> std::forward_iterator;
    };


    template<ResourceCollection Collection>
    class ResourceExporter
    {
    public:
        explicit ResourceExporter(const Collection& collection)
            : collection_(collection)
        {
        }


        [[nodiscard]] std::expected<ExportResult, std::string> exportAll(const ExportOptions& options) const
        {
            ExportResult result{};
            result.totalResources = collection_.size();

            if (result.totalResources == 0)
                return result;

            if (!fs::exists(options.outputDirectory))
            {
                std::error_code ec;
                fs::create_directories(options.outputDirectory, ec);
                if (ec)
                    return std::unexpected(std::format("Failed to create output directory: {}", ec.message()));
            }

            for (auto [id, data] : collection_)
            {
                if (!shouldExport(id, options))
                {
                    result.skipped++;
                    continue;
                }

                const auto outputPath = makeOutputPath(id, options.outputDirectory);

                if (!options.overwriteExisting && fs::exists(outputPath))
                {
                    result.skipped++;
                    continue;
                }

                if (auto writeResult = writeResource(data, outputPath))
                {
                    result.exported++;
                    result.totalBytes += data.size();
                }
                else
                {
                    result.failed++;
                    result.errors.push_back(std::format("{}: {}", id, writeResult.error()));
                }
            }
            return result;
        }


        std::expected<ExportResult, std::string> exportSingle(const ExportOptions& options,
                                                              const fs::path& outputPath) const;

    private:
        static fs::path makeOutputPath(std::string_view id, const fs::path& baseDir)
        {
            const bool isAudio = id.find('.') == std::string_view::npos;
            const fs::path outputDir = isAudio
                ? baseDir / "audio"
                : baseDir / "graphics";

            std::error_code ec;
            fs::create_directories(outputDir, ec);

            if (isAudio)
            {
                fs::path result = outputDir / std::string(id);
                result.replace_extension(".aac");
                return result;
            }

            return outputDir / id;
        }


        [[nodiscard]] static bool shouldExport(std::string_view id, const ExportOptions& options)
        {
            if (options.filter && !options.filter(id))
                return false;

            return true;
        }

        static std::expected<void, std::string> writeResource(const std::span<const uint8_t> data, const fs::path& path)
        {
            std::ofstream file(path, std::ios::out | std::ios::binary);
            if (!file)
                return std::unexpected(std::format("Failed to open file for writing: {}", path.string()));

            file.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));

            if (!file)
                return std::unexpected(std::format("Failed to write data to: {}", path.string()));

            return {};
        }

        const Collection& collection_;
    };
}

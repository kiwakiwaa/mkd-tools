//
// kiwakiwaaにより 2026/02/07 に作成されました。
//

#include "MKD/output/resource_exporter.hpp"
#include "MKD/output/export_accumulator.hpp"
#include "MKD/platform/parallel.hpp"

#include <mutex>
#include <pugixml.h>
#include <sstream>

namespace MKD
{
    namespace
    {
        std::error_code createOutputDirs(const ExportOptions& options, const ResourceType type)
        {
            std::error_code ec;
            fs::create_directories(options.outputDirectory / resourceTypeName(type), ec);
            return ec;
        }

        fs::path buildOutputPath(const ExportOptions& options, const ResourceType type, std::string_view filename)
        {
            if (options.createSubdirectories)
                return options.outputDirectory / resourceTypeName(type) / filename;
            return options.outputDirectory / filename;
        }
    }


    std::expected<ExportResult, std::string> ResourceExporter::exportAll(
        const Rsc& rsc, const ExportOptions& options, const ResourceType type)
    {
        if (rsc.empty())
            return ExportResult{};

        if (const auto ec = createOutputDirs(options, type))
            return std::unexpected(std::format("Failed to create output directories: {}", ec.message()));

        const auto ext = type == ResourceType::Entries ? ".xml" : ".aac";
        ExportAccumulator acc(rsc.size(), type, options.progressCallback);
        std::mutex readMutex;

        parallel_for(rsc.size(), [&](const size_t i) {
            uint32_t itemId;
            std::vector<uint8_t> owned;
            {
                std::lock_guard lock(readMutex);
                auto item = rsc.getByIndex(i);
                if (!item)
                {
                    acc.recordFailure(std::format("Index {}: {}", i, item.error()));
                    return;
                }
                itemId = item->itemId;
                owned.assign(item->data.begin(), item->data.end());
            }

            if (options.prettyPrintXml && type == ResourceType::Entries)
            {
                if (auto pretty = prettyPrintXml(owned); !pretty.empty())
                    owned = std::move(pretty);
            }

            const auto filename = std::format("{:06}.{}", itemId, ext);
            if (const auto outputPath = buildOutputPath(options, type, filename); shouldSkipExisting(outputPath, options.overwriteExisting))
                acc.recordSkip();
            else if (auto result = writeData(owned, outputPath))
                acc.recordExport(owned.size());
            else
                acc.recordFailure(std::format("Entry {}: {}", itemId, result.error()));
        });

        return acc.finalize();
    }


    std::expected<ExportResult, std::string> ResourceExporter::exportAll(
        const Nrsc& nrsc, const ExportOptions& options, const ResourceType type)
    {
        if (nrsc.empty())
            return ExportResult{};

        if (const auto ec = createOutputDirs(options, type))
            return std::unexpected(std::format("Failed to create output directories: {}", ec.message()));

        ExportAccumulator acc(nrsc.size(), type, options.progressCallback);
        std::mutex readMutex;

        parallel_for(nrsc.size(), [&](const size_t i) {
            std::string id;
            std::vector<uint8_t> owned;
            {
                std::lock_guard lock(readMutex);
                auto item = nrsc.getByIndex(i);
                if (!item)
                {
                    acc.recordFailure(std::format("Index {}: {}", i, item.error()));
                    return;
                }
                id = std::string(item->id);
                owned.assign(item->data.begin(), item->data.end());
            }

            // nrsc items with no extension in their ID are audio files
            const bool isAudio = id.find('.') == std::string::npos;
            auto filename = isAudio ? std::format("{}.aac", id) : id;
            if (auto outputPath = buildOutputPath(options, type, filename); shouldSkipExisting(outputPath, options.overwriteExisting))
                acc.recordSkip();
            else if (auto result = writeData(owned, outputPath))
                acc.recordExport(owned.size());
            else
                acc.recordFailure(std::format("{}: {}", id, result.error()));
        });

        return acc.finalize();
    }


    std::expected<ExportResult, std::string> ResourceExporter::exportFont(
        const Font& font, const ExportOptions& options)
    {
        ExportResult result;
        result.totalResources = 1;

        if (const auto ec = createOutputDirs(options, ResourceType::Fonts))
            return std::unexpected(std::format("Failed to create output directories: {}", ec.message()));

        auto fontDataResult = font.getData();
        if (!fontDataResult)
            return std::unexpected(std::format("Failed to get font data: {}", fontDataResult.error()));

        const auto fontData = *fontDataResult;
        if (fontData.empty())
        {
            result.skipped = 1;
            return result;
        }

        auto extension = font.detectType();
        if (!extension)
            return std::unexpected("Unrecognized font type");

        auto filename = std::format("{}.{}", font.name(), *extension);
        if (auto outputPath = buildOutputPath(options, ResourceType::Fonts, filename); shouldSkipExisting(outputPath, options.overwriteExisting))
        {
            result.skipped = 1;
        }
        else if (auto writeResult = writeData(fontData, outputPath))
        {
            result.exported = 1;
            result.totalBytes = fontData.size();
        }
        else
        {
            result.failed = 1;
            result.errors.push_back(
                std::format("Font export failed: {}", writeResult.error()));
        }

        return result;
    }


    std::vector<uint8_t> ResourceExporter::prettyPrintXml(const std::span<const uint8_t> data)
    {
        pugi::xml_document doc;
        if (!doc.load_buffer(data.data(), data.size()))
            return {};

        std::ostringstream oss;
        doc.save(oss, "  ", pugi::format_default, pugi::encoding_utf8);
        std::string str = oss.str();
        return {str.begin(), str.end()};
    }
}

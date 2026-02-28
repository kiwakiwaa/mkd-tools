//
// kiwakiwaaにより 2026/02/07 に作成されました。
//

#include "MKD/output/resource_exporter.hpp"
#include "MKD/output/export_accumulator.hpp"
#include "MKD/platform/parallel.hpp"

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

        fs::path buildOutputDir(const ExportOptions& options, const ResourceType type)
        {
            if (options.createSubdirectories)
                return options.outputDirectory / resourceTypeName(type);
            return options.outputDirectory;
        }
    }





    Result<ExportResult> ResourceExporter::exportAll(const Rsc& rsc, const ExportOptions& options, const ResourceType type)
    {
        if (rsc.empty())
            return ExportResult{};

        if (const auto ec = createOutputDirs(options, type))
            return std::unexpected(std::format("Failed to create output directories: {}", ec.message()));

        const auto ext = type == ResourceType::Entries ? ".xml" : ".aac";
        const auto baseDir = buildOutputDir(options, type);
        ExportAccumulator acc(rsc.size(), type, options.progressCallback);

#if defined(__APPLE__) || defined(__linux__)
        parallelFor(rsc.size(), [&](const size_t i) {
            auto item = rsc.getByIndex(i);
            if (!item)
            {
                acc.recordFailure(item.error());
                return;
            }

            const auto filename = std::format("{:06}{}", item->itemId, ext);
            if (const auto path = baseDir / filename; shouldSkipExisting(path, options.overwriteExisting))
                acc.recordSkip();
            else if (auto r = writeData(item->data, path))
                acc.recordExport(item->data.size());
            else
                acc.recordFailure(std::format("Entry {}: {}", item->itemId, r.error()));
        });
#else
        for (const auto [itemId, data] : rsc)
        {
            const auto filename = std::format("{:06}{}", itemId, ext);
            if (const auto path = baseDir / filename; shouldSkipExisting(path, options.overwriteExisting))
                acc.recordSkip();
            else if (auto r = writeData(data, path))
                acc.recordExport(data.size());
            else
                acc.recordFailure(std::format("Entry {}: {}", itemId, r.error()));
        }
#endif

        return acc.finalize();
    }


    Result<ExportResult> ResourceExporter::exportAll(
        const Nrsc& nrsc, const ExportOptions& options, const ResourceType type)
    {
        if (nrsc.empty())
            return ExportResult{};

        if (const auto ec = createOutputDirs(options, type))
            return std::unexpected(std::format("Failed to create output directories: {}", ec.message()));

        const auto baseDir = buildOutputDir(options, type);
        ExportAccumulator acc(nrsc.size(), type, options.progressCallback);

#if defined(__APPLE__) || defined(__linux__)
        parallelFor(nrsc.size(), [&](const size_t i) {
            auto item = nrsc.getByIndex(i);
            if (!item)
            {
                acc.recordFailure(std::format("Index {}: {}", i, item.error()));
                return;
            }

            // nrsc items with no extension in their ID are audio files
            const bool isAudio = item->id.find('.') == std::string::npos;
            const auto filename = isAudio ? std::format("{}.aac", item->id) : std::string(item->id);
            if (const auto outputPath = baseDir / filename; shouldSkipExisting(outputPath, options.overwriteExisting))
                acc.recordSkip();
            else if (auto result = writeData(item->data, outputPath))
                acc.recordExport(item->data.size());
            else
                acc.recordFailure(std::format("{}: {}", item->id, result.error()));
        });
#else
        for (const auto [itemId, data] : nrsc)
        {
            const bool isAudio = itemId.find('.') == std::string::npos;
            const auto filename = isAudio ? std::format("{}.aac", itemId) : std::string(itemId);
            if (const auto path = baseDir / filename; shouldSkipExisting(path, options.overwriteExisting))
                acc.recordSkip();
            else if (auto r = writeData(data, path))
                acc.recordExport(data.size());
            else
                acc.recordFailure(std::format("Entry {}: {}", itemId, r.error()));
        }
#endif

        return acc.finalize();
    }


    Result<ExportResult> ResourceExporter::exportFont(
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

        auto baseDir = buildOutputDir(options, ResourceType::Fonts);
        auto filename = std::format("{}.{}", font.name(), *extension);
        if (auto outputPath = baseDir / filename; shouldSkipExisting(outputPath, options.overwriteExisting))
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

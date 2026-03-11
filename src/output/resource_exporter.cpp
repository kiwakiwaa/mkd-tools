//
// kiwakiwaaにより 2026/02/07 に作成されました。
//

#include "MKD/output/resource_exporter.hpp"
#include "export_accumulator.hpp"
#include "platform/parallel.hpp"

#include <format>
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

        template<typename F>
        void forEachItem(const size_t count, F&& fn)
        {
#if defined(__APPLE__) || defined(__linux__)
            parallelFor(count, std::forward<F>(fn));
#else
            for (size_t i = 0; i < count; ++i)
                fn(i);
#endif
        }
    }


    Result<ExportResult> ResourceExporter::exportAll(const ResourceStore& resourceStore, const ExportOptions& options,
                                                     const ResourceType type)
    {
        if (resourceStore.empty())
            return ExportResult{};

        if (const auto ec = createOutputDirs(options, type))
            return std::unexpected(std::format("Failed to create output directories: {}", ec.message()));

        const auto ext = type == ResourceType::Contents ? ".xml" : ".aac";
        const auto baseDir = buildOutputDir(options, type);
        const bool shouldFormat = type == ResourceType::Contents && options.prettyPrintXml;
        ExportAccumulator acc(resourceStore.size(), type, options.progressCallback);

        forEachItem(resourceStore.size(), [&](const size_t i) {
            auto item = resourceStore.getByIndex(i);
            if (!item)
            {
                acc.recordFailure(item.error());
                return;
            }

            const auto filename = std::format("{:06}{}", item->itemId, ext);
            const auto path = baseDir / filename;
            if (shouldSkipExisting(path, options.overwriteExisting))
            {
                acc.recordSkip();
                return;
            }

            std::vector<uint8_t> buf;
            const auto output = formatIfNeeded(item->data.span(), buf, shouldFormat);

            if (auto r = writeData(output, path))
                acc.recordExport(output.size());
            else
                acc.recordFailure(std::format("Entry {}: {}", item->itemId, r.error()));
        });

        return acc.finalize();
    }


    Result<ExportResult> ResourceExporter::exportAll(
        const NamedResourceStore& namedResourceStore, const ExportOptions& options, const ResourceType type)
    {
        if (namedResourceStore.empty())
            return ExportResult{};

        if (const auto ec = createOutputDirs(options, type))
            return std::unexpected(std::format("Failed to create output directories: {}", ec.message()));

        const auto baseDir = buildOutputDir(options, type);
        ExportAccumulator acc(namedResourceStore.size(), type, options.progressCallback);

        forEachItem(namedResourceStore.size(), [&](const size_t i) {
            auto item = namedResourceStore.getByIndex(i);
            if (!item)
            {
                acc.recordFailure(std::format("Index {}: {}", i, item.error()));
                return;
            }

            const bool isAudio = item->id.find('.') == std::string::npos;
            const auto filename = isAudio ? std::format("{}.aac", item->id) : std::string(item->id);
            if (const auto outputPath = baseDir / filename; shouldSkipExisting(outputPath, options.overwriteExisting))
                acc.recordSkip();
            else if (auto result = writeData(item->data.span(), outputPath))
                acc.recordExport(item->data.size());
            else
                acc.recordFailure(std::format("{}: {}", item->id, result.error()));
        });

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


    std::span<const uint8_t> ResourceExporter::formatIfNeeded(const std::span<const uint8_t> data,
                                                              std::vector<uint8_t>& buf, bool shouldFormat)
    {
        if (shouldFormat)
        {
            buf = prettyPrintXml(data);
            if (!buf.empty())
                return buf;
        }
        return data;
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

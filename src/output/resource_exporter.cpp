//
// kiwakiwaaにより 2026/02/07 に作成されました。
//

#include "monokakido/output/resource_exporter.hpp"

#include <pugixml.h>
#include <sstream>

namespace monokakido
{
    std::expected<ExportResult, std::string> ResourceExporter::exportAll(const Rsc& rsc, const ExportOptions& options)
    {
        ExportResult result;
        result.totalResources = rsc.size();

        if (result.totalResources == 0)
            return result;

        std::vector<uint8_t> buffer;

        for (const auto& [itemId, data] : rsc)
        {
            // Detect if this is audio or XML
            auto [subdir, filename, finalData] = prepareRscItem(itemId, data, options, buffer);
            fs::path outputDir = options.createSubdirectories
                ? options.outputDirectory / subdir
                : options.outputDirectory;

            fs::path outputPath = outputDir / filename;

            if (shouldSkipExisting(outputPath, options.overwriteExisting))
            {
                result.skipped++;
                continue;
            }

            if (auto writeResult = writeData(finalData, outputPath))
            {
                result.exported++;
                result.totalBytes += finalData.size();
            }
            else
            {
                result.failed++;
                result.errors.push_back(std::format("Entry {}: {}", itemId, writeResult.error()));
            }
        }

        return result;
    }


    std::expected<ExportResult, std::string> ResourceExporter::exportAll(const Nrsc& nrsc, const ExportOptions& options)
    {
        ExportResult result;
        result.totalResources = nrsc.size();

        if (result.totalResources == 0)
            return result;

        for (const auto& [id, data] : nrsc)
        {
            const bool isAudio = id.find('.') == std::string_view::npos;

            fs::path outputDir = options.outputDirectory;
            if (options.createSubdirectories)
                outputDir /= isAudio ? "audio" : "graphics";

            fs::path outputPath = outputDir / (isAudio ? std::format("{}.aac", id) : std::string(id));

            if (shouldSkipExisting(outputPath, options.overwriteExisting))
            {
                result.skipped++;
                continue;
            }

            if (auto writeResult = writeData(data, outputPath))
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



    std::expected<ExportResult, std::string> ResourceExporter::exportFont(const Font& font, const ExportOptions& options)
    {
        ExportResult result;
        result.totalResources = 1;

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

        const fs::path outputDir = options.createSubdirectories
                                       ? options.outputDirectory / "fonts"
                                       : options.outputDirectory;

        const fs::path outputPath = outputDir / std::format("{}.{}", font.name(), extension.value());

        if (shouldSkipExisting(outputPath, options.overwriteExisting))
        {
            result.skipped = 1;
            return result;
        }

        if (auto writeResult = writeData(fontData, outputPath))
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



    std::tuple<std::string, std::string, std::span<const uint8_t>> ResourceExporter::prepareRscItem(
        uint32_t itemId, std::span<const uint8_t> data, const ExportOptions& options, std::vector<uint8_t>& buffer)
    {
        buffer.clear();

        // Check if it's audio data
        if (isAudioData(data))
            return {"audio", std::format("{:010}.aac", itemId), data};

        // It's XML
        if (options.prettyPrintXml)
        {
            if (pugi::xml_document doc; doc.load_buffer(data.data(), data.size()))
            {
                std::ostringstream oss;
                doc.save(oss, "  ", pugi::format_default, pugi::encoding_utf8);
                std::string prettyXml = oss.str();
                buffer.assign(prettyXml.begin(), prettyXml.end());
                return {"entries", std::format("{:010}.xml", itemId), buffer};
            }
        }

        return {"entries", std::format("{:010}.xml", itemId), data};
    }


    bool ResourceExporter::isAudioData(const std::span<const uint8_t> data)
    {
        if (data.size() < 8)
            return false;

        // ADTS header (FF F1 or FF F9)
        if (data[0] == 0xFF && (data[1] == 0xF1 || data[1] == 0xF9))
            return true;

        return false;
    }
}

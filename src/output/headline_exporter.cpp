//
// kiwakiwaaにより 2026/02/14 に作成されました。
//

#include "MKD/output/headline_exporter.hpp"

#include <format>
#include <fstream>

namespace MKD
{
    Result<ExportResult> HeadlineExporter::exportHeadlines(const HeadlineStore& store, const ExportOptions& options)
    {
        ExportResult result;
        result.totalResources = store.size();

        if (result.totalResources == 0)
            return result;

        const fs::path dir = options.createSubdirectories
                                 ? options.outputDirectory / "Headline"
                                 : options.outputDirectory;
        fs::path outputPath = dir / std::format("{}.tsv", store.filename());

        if (options.createSubdirectories)
            fs::create_directories(dir);

        if (shouldSkipExisting(outputPath, options.overwriteExisting))
        {
            result.skipped = result.totalResources;
            return result;
        }

        std::ofstream out(outputPath, std::ios::binary);
        if (!out)
        {
            return std::unexpected(
                std::format("Failed to create output file: {}", outputPath.string())
            );
        }

        for (const auto& components : store)
        {
            if (auto rowResult = writeRow(out, components); !rowResult)
            {
                result.failed++;
                result.errors.push_back(
                    std::format("Entry {}-{}: {}",
                                components.entryId.pageId,
                                components.entryId.itemId,
                                rowResult.error()));
            }
            else
            {
                result.exported++;
                result.totalBytes += components.headlineUtf8().size()
                        + components.prefixUtf8().size()
                        + components.suffixUtf8().size() + 4;
            }
        }

        return result;
    }


    Result<void> HeadlineExporter::writeRow(std::ofstream& out, const HeadlineComponents& components)
    {
        out << std::format("{:06}-{:04X}", components.entryId.pageId, components.entryId.itemId) << '\t';
        if (!components.headline.empty())
            out << components.headlineUtf8() << '\t';

        if (!components.prefix.empty())
            out << components.prefixUtf8() << '\t';

        if (!components.suffix.empty())
            out << components.suffixUtf8() << '\t';

        out << '\n';

        if (!out)
        {
            return std::unexpected("Failed to write TSV row");
        }
        return {};
    }
}

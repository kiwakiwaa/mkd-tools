//
// kiwakiwaaにより 2026/02/20 に作成されました。
//

#include "MKDCLI/commands.hpp"
#include "MKDCLI/format.hpp"
#include "MKDCLI/progress.hpp"

#include "MKD/dictionary/dictionary_product.hpp"

#include <algorithm>
#include <iostream>

namespace MKDCLI
{
    int runList(MKD::DictionarySource& source, const CLIOptions& opts)
    {
        auto available = source.findAllAvailable();
        if (!available)
        {
            std::cerr << Colour::red("Error: ") << available.error() << "\n";
            return 1;
        }

        if (available->empty())
        {
            std::cerr << "No dictionaries found.\n";
            return 0;
        }

        std::ranges::sort(*available, [](const auto& a, const auto& b) {
            return a.id < b.id;
        });

        std::cerr << Colour::bold("Available dictionaries") << Colour::dim(" (" + std::to_string(available->size()) + " found)") << "\n\n";

        std::vector<TableRow> rows;
        for (const auto& [productId, path] : *available)
        {
            auto meta = MKD::DictionaryMetadata::loadFromPath(
                path / "Contents" / (path.stem().string() + ".json")
            );

            TableRow row;
            row.cells.push_back(Colour::cyan(productId));

            if (!meta)
            {
                row.cells.push_back(Colour::dim("(metadata unavailable)"));
                row.cells.emplace_back("");
                row.cells.emplace_back("");
            }
            else
            {
                row.cells.push_back(meta->displayName->ja.value_or("-"));
                row.cells.push_back(Colour::dim(meta->genre->ja.value_or("")));
            }

            rows.push_back(std::move(row));
        }

        printTable(rows);
        std::cerr << "\n";
        return 0;
    }


    int runInfo(const MKD::DictionarySource& source, const CLIOptions& opts)
    {
        auto info = source.findById(opts.dictId);
        if (!info)
        {
            std::cerr << Colour::red("Error: ") << info.error() << "\n";
            return 1;
        }

        auto product = MKD::DictionaryProduct::openAtPath(info->path);
        if (!product)
        {
            std::cerr << Colour::red("Error: ") << product.error() << "\n";
            return 1;
        }

        const auto& meta = product->metadata();

        // product info
        std::cerr << Colour::bold(opts.dictId) << "\n\n";

        auto printField = [](std::string_view label, const std::optional<std::string>& value) {
            std::cerr << "  " << Colour::dim(std::string(label)) << "  "
                    << value.value_or("-") << "\n";
        };

        auto localizedOr = [](const std::optional<MKD::LocalizedString>& ls) -> std::optional<std::string> {
            if (!ls) return std::nullopt;
            if (ls->ja) return ls->ja;
            if (ls->en) return ls->en;
            return std::nullopt;
        };

        printField("Name:       ", localizedOr(meta.displayName));
        printField("Description:", localizedOr(meta.description));
        printField("Category:   ", meta.category);
        printField("Version:    ", meta.version);

        // content info
        const auto& dicts = product->dictionaries();
        std::cerr << "\n  " << Colour::dim("Contents:")
                << Colour::dim(" (" + std::to_string(dicts.size()) + ")") << "\n";

        for (const auto& dict : dicts)
        {
            const auto& content = dict.content();

            std::cerr << "\n    " << Colour::bold(content.identifier) << "\n";

            auto printContentField = [](std::string_view label, const std::optional<std::string>& value) {
                std::cerr << "      " << Colour::dim(std::string(label)) << "  "
                        << value.value_or("-") << "\n";
            };

            auto contentLocalized = [](const std::optional<MKD::LocalizedString>& ls) -> std::optional<std::string> {
                if (!ls) return std::nullopt;
                if (ls->ja) return ls->ja;
                if (ls->en) return ls->en;
                return std::nullopt;
            };

            printContentField("Title:    ", contentLocalized(content.title));
            printContentField("Publisher:", contentLocalized(content.publisher));
            printContentField("Edition:  ", contentLocalized(content.edition));
            printContentField("Directory:", std::optional(content.directory));

            // Resource availability for this content
            std::cerr << "\n      " << Colour::dim("Resources:") << "\n";

            auto resourceLine = [](std::string_view name, const size_t count) {
                std::cerr << "        " << (count > 0 ? Colour::green("●") : Colour::dim("○"))
                        << " " << name;
                if (count > 0)
                    std::cerr << Colour::dim(" (" + std::to_string(count) + ")");
                std::cerr << "\n";
            };

            resourceLine("Entries", dict.resourceCount(MKD::ResourceType::Entries));
            resourceLine("Audio", dict.resourceCount(MKD::ResourceType::Audio));
            resourceLine("Graphics", dict.resourceCount(MKD::ResourceType::Graphics));
            resourceLine("Fonts", dict.resourceCount(MKD::ResourceType::Fonts));
            resourceLine("Keystores", dict.resourceCount(MKD::ResourceType::Keystores));
            resourceLine("Headlines", dict.resourceCount(MKD::ResourceType::Headlines));
        }

        std::cerr << "\n";
        return 0;
    }


    int runExport(const MKD::DictionarySource& source, const CLIOptions& opts,
                  const MKD::ExportOptions& exportOpts)
    {
        auto info = source.findById(opts.dictId);
        if (!info)
        {
            std::cerr << Colour::red("Error: ") << info.error() << "\n";
            return 1;
        }

        auto product = MKD::DictionaryProduct::openAtPath(info->path);
        if (!product)
        {
            std::cerr << Colour::red("Error: ") << product.error() << "\n";
            return 1;
        }

        const auto& dicts = product->dictionaries();
        if (dicts.empty())
        {
            std::cerr << Colour::red("Error: ") << "No content entries found in product.\n";
            return 1;
        }

        MKD::ExportResult totalResult;
        bool anyFailed = false;

        for (const auto& dict : dicts)
        {
            std::cerr << Colour::bold("Exporting content: ") << dict.id() << "\n";

            auto options = exportOpts;
            ExportProgress progress;
            options.progressCallback = [&progress](const MKD::ExportEvent& event) {
                progress.onEvent(event);
            };

            auto result = dict.exportWithOptions(options);

            progress.finish();
            totalResult += result;

            if (!result.isSuccess())
                anyFailed = true;
        }

        if (dicts.size() > 1)
            std::cerr << "\n" << Colour::bold("Combined results:") << "\n";

        printExportSummary(totalResult);

        return anyFailed ? 1 : 0;
    }
}

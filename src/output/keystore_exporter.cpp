//
// kiwakiwaaにより 2026/02/14 に作成されました。
//

#include "MKD/output/keystore_exporter.hpp"

#include <algorithm>
#include <format>
#include <fstream>
#include <unordered_map>

namespace MKD
{
    Result<ExportResult> KeystoreExporter::exportKeystore(const Keystore& keystore, const ExportOptions& options)
    {
        auto entries = collectEntries(keystore, KeystoreIndex::Prefix);
        if (!entries)
            return std::unexpected(entries.error());

        ExportResult combined;

        const fs::path dir = options.createSubdirectories
                                 ? options.outputDirectory / "Keystore"
                                 : options.outputDirectory;

        if (hasFlag(options.keystoreExportMode, KeystoreExportMode::Forward))
        {
            const fs::path path = dir / std::format("{}.tsv", keystore.filename());
            if (shouldSkipExisting(path, options.overwriteExisting))
            {
                combined.skipped++;
            }
            else if (auto r = writeForward(*entries, path))
            {
                combined += *r;
            }
            else
            {
                return std::unexpected(std::format("Forward export failed: {}", r.error()));
            }
        }

        if (hasFlag(options.keystoreExportMode, KeystoreExportMode::Inverse))
        {
            const fs::path path = dir / std::format("{}_inverse.tsv", keystore.filename());
            if (shouldSkipExisting(path, options.overwriteExisting))
            {
                combined.skipped++;
            }
            else if (auto r = writeInverse(*entries, path))
            {
                combined += *r;
            }
            else
            {
                return std::unexpected(std::format("Inverse export failed: {}", r.error()));
            }
        }

        return combined;
    }


    Result<std::vector<KeystoreExporter::ForwardEntry>> KeystoreExporter::collectEntries(const Keystore& keystore, const KeystoreIndex index)
    {
        const size_t count = keystore.indexSize(index);
        std::vector<ForwardEntry> entries;
        entries.reserve(count);

        for (size_t i = 0; i < count; ++i)
        {
            auto result = keystore.getByIndex(index, i);
            if (!result)
                return std::unexpected(std::format("Entry {}: {}", i, result.error()));

            entries.push_back({
                .key = result->key,
                .pages = std::move(result->pages),
            });
        }

        return entries;
    }


    Result<ExportResult> KeystoreExporter::writeForward(std::span<const ForwardEntry> entries, const fs::path& path)
    {
        std::error_code ec;
        fs::create_directories(path.parent_path(), ec);
        if (ec)
            return std::unexpected(std::format("Cannot create directory: {}", ec.message()));

        std::ofstream out(path, std::ios::binary);
        if (!out)
            return std::unexpected(std::format("Cannot open {}", path.string()));

        ExportResult result;
        result.totalResources = entries.size();

        for (const auto& [key, pages] : entries)
        {
            out << key << '\t';
            for (size_t i = 0; i < pages.size(); ++i)
            {
                if (i > 0) out << '\t';
                out << pages[i].page << '-' << pages[i].item;
            }
            out << '\n';
            result.exported++;
        }

        result.totalBytes = static_cast<uint64_t>(out.tellp());
        return result;
    }


    Result<ExportResult> KeystoreExporter::writeInverse(std::span<const ForwardEntry> entries, const fs::path& path)
    {
        std::error_code ec;
        fs::create_directories(path.parent_path(), ec);
        if (ec)
            return std::unexpected(std::format("Cannot create directory: {}", ec.message()));

        // Pack (page, item) into a single uint64 for fast hashing.
        // Upper 32 bits = page, lower 16 bits = item.
        constexpr auto pack = [](const PageReference& ref) -> uint64_t {
            return (static_cast<uint64_t>(ref.page) << 16) | ref.item;
        };

        std::unordered_map<uint64_t, std::vector<std::string_view> > inverse;
        inverse.reserve(entries.size());

        for (const auto& [key, pages] : entries)
        {
            for (const auto& ref : pages)
                inverse[pack(ref)].push_back(key);
        }

        // Sort by packed key (page-major order)
        std::vector<std::pair<uint64_t, std::vector<std::string_view>*> > sorted;
        sorted.reserve(inverse.size());
        for (auto& [packed, keys] : inverse)
            sorted.emplace_back(packed, &keys);

        std::ranges::sort(sorted, [](const auto& a, const auto& b) { return a.first < b.first; });

        std::ofstream out(path, std::ios::binary);
        if (!out)
            return std::unexpected(std::format("Cannot open {}", path.string()));

        ExportResult result;
        result.totalResources = sorted.size();

        for (const auto& [packed, keys] : sorted)
        {
            const auto page = static_cast<uint32_t>(packed >> 16);
            const auto item = static_cast<uint16_t>(packed & 0xFFFF);

            out << std::format("{:06}-{:04X}", page, item) <<'\t';
            for (size_t i = 0; i < keys->size(); ++i)
            {
                if (i > 0) out << '\t';
                out << (*keys)[i];
            }
            out << '\n';
            result.exported++;
        }

        result.totalBytes = static_cast<uint64_t>(out.tellp());
        return result;
    }
}

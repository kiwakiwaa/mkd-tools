//
// kiwakiwaaにより 2026/02/14 に作成されました。
//

#pragma once

#include "MKD/output/export_options.hpp"
#include "MKD/output/export_result.hpp"
#include "MKD/output/base_exporter.hpp"
#include "MKD/resource/keystore.hpp"

namespace MKD
{
    [[nodiscard]] constexpr KeystoreExportMode operator|(KeystoreExportMode a, KeystoreExportMode b) noexcept
    {
        return static_cast<KeystoreExportMode>(
            static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
    }

    [[nodiscard]] constexpr bool hasFlag(KeystoreExportMode mode, KeystoreExportMode flag) noexcept
    {
        return (static_cast<uint8_t>(mode) & static_cast<uint8_t>(flag)) != 0;
    }

    class KeystoreExporter : BaseExporter
    {
    public:
        /**
         * Export a keystore's mappings to TSV files.
         *
         * Forward format (one line per key):
         *   <key>\t<page>-<entry>[,<page>-<entry>...]
         *
         * Inverse format (one line per entry id):
         *   <page>-<entry>\t<key>[,<key>,...]
         *
         * The inverse map is built with a hash map over packed
         * entry ids, then sorted by page ID
         */
        static Result<ExportResult> exportKeystore(const Keystore& keystore, const ExportOptions& options);

    private:
        struct ForwardEntry
        {
            std::string_view key;
            std::vector<EntryId> entryIds;
        };

        static Result<std::vector<ForwardEntry>> collectEntries(const Keystore& keystore, KeystoreIndex index);

        static Result<ExportResult> writeForward(std::span<const ForwardEntry> entries, const fs::path& path);

        static Result<ExportResult> writeInverse(std::span<const ForwardEntry> entries, const fs::path& path);
    };
}

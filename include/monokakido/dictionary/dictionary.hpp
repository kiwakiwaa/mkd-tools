//
// kiwakiwaaにより 2026/01/15 に作成されました。
//

#pragma once

#include "metadata.hpp"
#include "paths.hpp"
#include "monokakido/platform/dictionary_source.hpp"
#include "monokakido/resource/nrsc/nrsc.hpp"
#include "monokakido/resource/rsc/rsc.hpp"
#include "monokakido/resource/font.hpp"
#include "monokakido/resource/keystore/keystore.hpp"
#include "monokakido/resource/headline/headline_store.hpp"
#include "monokakido/output/resource_exporter.hpp"
#include "monokakido/output/keystore_exporter.hpp"
#include "monokakido/output/headline_exporter.hpp"

#include <variant>

namespace monokakido
{

    class Dictionary
    {
    public:

        static std::expected<Dictionary, std::string> open(std::string_view dictId, const DictionarySource& source);

        [[nodiscard]] const std::string& id() const noexcept;
        [[nodiscard]] const DictionaryMetadata& metadata() const noexcept;

        // returns nullptr if no nrsc resources available
        [[nodiscard]] Nrsc* graphics() noexcept;
        [[nodiscard]] const Nrsc* graphics() const noexcept;
        [[nodiscard]] std::vector<Font>& fonts() noexcept;
        [[nodiscard]] const std::vector<Font>& fonts() const noexcept;

        [[nodiscard]] bool hasAudio() const noexcept;
        [[nodiscard]] bool hasGraphics() const noexcept;
        [[nodiscard]] bool hasFonts() const noexcept;
        [[nodiscard]] bool hasKeystores() const noexcept;
        [[nodiscard]] bool hasHeadlines() const noexcept;

        ExportResult exportAll(const ExportOptions& options) const;
        std::expected<ExportResult, std::string> exportAudio(const ExportOptions& options) const;
        std::expected<ExportResult, std::string> exportEntries(const ExportOptions& options) const;
        std::expected<ExportResult, std::string> exportGraphics(const ExportOptions& options) const;
        std::expected<ExportResult, std::string> exportFonts(const ExportOptions& options) const;
        std::expected<ExportResult, std::string> exportKeystores(const ExportOptions& options) const;
        std::expected<ExportResult, std::string> exportHeadlines(const ExportOptions& options) const;


    private:

        static std::expected<Dictionary, std::string> openAtPath(const fs::path& path);

        Dictionary(
            std::string id,
            DictionaryMetadata metadata,
            DictionaryPaths paths,
            std::optional<Rsc> entries,
            std::optional<Nrsc> graphics,
            std::optional<std::variant<Rsc, Nrsc>> audio,
            std::vector<Font> fonts,
            std::vector<Keystore> keystores,
            std::vector<HeadlineStore> headlines);

        std::string id_;
        DictionaryPaths paths_;
        DictionaryMetadata metadata_;
        std::optional<Rsc> entries_;
        std::optional<Nrsc> graphics_;
        std::optional<std::variant<Rsc, Nrsc>> audio_;
        std::vector<Font> fonts_;
        std::vector<Keystore> keystores_;
        std::vector<HeadlineStore> headlines_;
    };
}

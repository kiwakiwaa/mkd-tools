//
// kiwakiwaaにより 2026/01/15 に作成されました。
//

#pragma once

#include "metadata.hpp"
#include "paths.hpp"
#include "monokakido/resource/nrsc/nrsc.hpp"
#include "monokakido/resource/rsc/rsc.hpp"
#include "monokakido/output/resource_exporter.hpp"
#include "monokakido/resource/font.hpp"

#include <variant>

namespace monokakido
{

    class Dictionary
    {
    public:

        static std::expected<Dictionary, std::string> open(std::string_view dictId);

        static std::expected<Dictionary, std::string> openAtPath(const fs::path& path);

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

        ExportResult exportAll(const ExportOptions& options) const;
        std::expected<ExportResult, std::string> exportAudio(const ExportOptions& options) const;
        std::expected<ExportResult, std::string> exportEntries(const ExportOptions& options) const;
        std::expected<ExportResult, std::string> exportGraphics(const ExportOptions& options) const;
        std::expected<ExportResult, std::string> exportFonts(const ExportOptions& options) const;


    private:

        Dictionary(
            std::string id,
            DictionaryMetadata metadata,
            DictionaryPaths paths,
            std::optional<Rsc> entries,
            std::optional<Nrsc> graphics,
            std::optional<std::variant<Rsc, Nrsc>> audio,
            std::vector<Font> fonts);

        std::string id_;
        DictionaryPaths paths_;
        DictionaryMetadata metadata_;
        std::optional<Rsc> entries_;
        std::optional<Nrsc> graphics_;
        std::optional<std::variant<Rsc, Nrsc>> audio_;
        std::vector<Font> fonts_;
    };
}
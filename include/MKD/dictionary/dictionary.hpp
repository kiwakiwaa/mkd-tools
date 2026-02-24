//
// kiwakiwaaにより 2026/01/15 に作成されました。
//

#pragma once

#include "metadata.hpp"
#include "paths.hpp"
#include "MKD/platform/dictionary_source.hpp"
#include "MKD/resource/nrsc/nrsc.hpp"
#include "MKD/resource/rsc/rsc.hpp"
#include "MKD/resource/font.hpp"
#include "MKD/resource/keystore/keystore.hpp"
#include "MKD/resource/headline/headline_store.hpp"
#include "MKD/output/export_options.hpp"
#include "MKD/output/resource_exporter.hpp"
#include "MKD/output/keystore_exporter.hpp"
#include "MKD/output/headline_exporter.hpp"

#include <variant>

namespace MKD
{
    class Dictionary
    {
    public:
        explicit Dictionary(DictionaryContent content,
                            std::optional<Rsc> entries,
                            std::optional<Nrsc> graphics,
                            std::optional<std::variant<Rsc, Nrsc> > audio,
                            std::vector<Font> fonts,
                            std::vector<Keystore> keystores,
                            std::vector<HeadlineStore> headlines);

        [[nodiscard]] const std::string& id() const noexcept;
        [[nodiscard]] const DictionaryContent& content() const noexcept;

        // returns nullptr if no nrsc resources available
        [[nodiscard]] Nrsc* graphics() noexcept;
        [[nodiscard]] const Nrsc* graphics() const noexcept;
        [[nodiscard]] std::vector<Font>& fonts() noexcept;
        [[nodiscard]] const std::vector<Font>& fonts() const noexcept;

        [[nodiscard]] size_t resourceCount(ResourceType type) const noexcept;

        ExportResult exportWithOptions(const ExportOptions& options) const;

    private:
        std::expected<ExportResult, std::string> exportAudio(const ExportOptions& options) const;
        std::expected<ExportResult, std::string> exportFonts(const ExportOptions& options) const;
        std::expected<ExportResult, std::string> exportKeystores(const ExportOptions& options) const;
        std::expected<ExportResult, std::string> exportHeadlines(const ExportOptions& options) const;

        DictionaryContent content_;
        std::optional<Rsc> entries_;
        std::optional<Nrsc> graphics_;
        std::optional<std::variant<Rsc, Nrsc> > audio_;
        std::vector<Font> fonts_;
        std::vector<Keystore> keystores_;
        std::vector<HeadlineStore> headlines_;
    };
}

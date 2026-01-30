//
// kiwakiwaaにより 2026/01/15 に作成されました。
//

#pragma once

#include "metadata.hpp"
#include "paths.hpp"
#include "monokakido/resource/nrsc/nrsc.hpp"
#include "monokakido/output/exporter.hpp"

namespace monokakido::dictionary
{

    class Dictionary
    {
    public:

        static std::expected<Dictionary, std::string> open(std::string_view dictId);

        static std::expected<Dictionary, std::string> openAtPath(const fs::path& path);

        [[nodiscard]] const std::string& id() const noexcept;
        [[nodiscard]] const DictionaryMetadata& metadata() const noexcept;

        // returns nullptr if no nrsc resources available
        [[nodiscard]] resource::Nrsc* graphics() noexcept;
        [[nodiscard]] const resource::Nrsc* graphics() const noexcept;

        [[nodiscard]] bool hasGraphics() const noexcept;


        std::expected<resource::ExportResult, std::string> exportAllResources() const;

        void print() const;


    private:

        Dictionary(std::string id, DictionaryMetadata metadata, DictionaryPaths paths, std::optional<resource::Nrsc> graphics, std::optional<resource::Nrsc> audio);

        std::string id_;
        DictionaryPaths paths_;
        DictionaryMetadata metadata_;

        std::optional<resource::Nrsc> graphics_;
        std::optional<resource::Nrsc> audio_;
        // resource::Rsc entryContent_; // future

    };
}
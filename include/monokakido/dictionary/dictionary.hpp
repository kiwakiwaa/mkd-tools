//
// Caoimheにより 2026/01/15 に作成されました。
//

#pragma once

#include "metadata.hpp"
#include "paths.hpp"
#include "monokakido/resource/nrsc/nrsc.hpp"
#include "monokakido/resource/exporter.hpp"

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
        [[nodiscard]] resource::Nrsc* mediaResources() noexcept;
        [[nodiscard]] const resource::Nrsc* mediaResources() const noexcept;

        [[nodiscard]] bool hasMediaResources() const noexcept;


        std::expected<resource::ExportResult, std::string> exportAllResources() const;

        void print() const;


    private:

        Dictionary(std::string id, DictionaryMetadata metadata, DictionaryPaths paths);

        std::string id_;
        DictionaryPaths paths_;
        DictionaryMetadata metadata_;

        // should separate audio & graphics later because a dictionary can have both
        // and they are always in separate folders
        std::optional<resource::Nrsc> mediaResources_;
        // resource::Rsc entryContent_; // future

    };

}
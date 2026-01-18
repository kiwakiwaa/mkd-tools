//
// Caoimheにより 2026/01/15 に作成されました。
//


#include "monokakido/dictionary/dictionary.hpp"
#include "monokakido/dictionary/catalog.hpp"

#include <iostream>
#include <utility>

namespace monokakido::dictionary
{
    std::expected<Dictionary, std::string> Dictionary::open(std::string_view dictId)
    {
        const auto& catalog = DictionaryCatalog::instance();

        const auto dictInfo = catalog.findById(dictId);
        if (!dictInfo)
            return std::unexpected(std::format("Dictionary '{}' not found", dictId));

        return openAtPath(dictInfo->path);
    }


    std::expected<Dictionary, std::string> Dictionary::openAtPath(const fs::path& path)
    {
        auto dictId = path.stem().string();

        auto metadata = DictionaryMetadata::loadFromPath(
            path / "Contents" / (dictId + ".json")
        );
        if (!metadata)
            return std::unexpected(std::format("Failed to load dictionary metadata: '{}'", metadata.error()));

        auto paths = DictionaryPaths::create(path, metadata.value());
        if (!paths)
            return std::unexpected(std::format("Failed to load paths: '{}'", paths.error()));

        return Dictionary(
            std::string(dictId),
            std::move(metadata.value()),
            std::move(paths.value())
        );
    }


    const std::string& Dictionary::id() const noexcept
    {
        return id_;
    }


    const DictionaryMetadata& Dictionary::metadata() const noexcept
    {
        return metadata_;
    }


    resource::Nrsc* Dictionary::mediaResources() noexcept
    {
        if (!mediaResources_ || mediaResources_->size() == 0)
            return nullptr;

        return &*mediaResources_;
    }


    const resource::Nrsc* Dictionary::mediaResources() const noexcept
    {
        if (!mediaResources_ || mediaResources_->size() == 0)
            return nullptr;

        return &*mediaResources_;
    }


    bool Dictionary::hasMediaResources() const noexcept
    {
        return mediaResources_ && mediaResources_->size() > 0;
    }


    Dictionary::Dictionary(std::string id, DictionaryMetadata metadata, DictionaryPaths paths)
        : id_(std::move(id)), paths_(std::move(paths)), metadata_(std::move(metadata))
    {
    }


    void Dictionary::print() const
    {
        std::cout << "-------" << metadata_.displayName().value_or("[Display Name]") << "-----\n";
        std::cout << std::format("ID: {}", id_) << '\n';
        std::cout << std::format("Description: {}", metadata_.description().value_or("[Missing]")) << '\n';
        std::cout << std::format("Publisher: {}", metadata_.publisher().value_or("[Missing]")) << '\n';
        std::cout << std::format("Content dir: {}",
                                 metadata_.contentDirectoryName().value_or(fs::path("[Missing]")).string()) <<
                std::endl;
    }


    std::expected<resource::ExportResult, std::string> Dictionary::exportAllResources() const
    {
        auto nrscResult = resource::Nrsc::open(paths_.resolve(PathType::Graphics));
        if (!nrscResult)
        {
            return std::unexpected(nrscResult.error());
        }

        const auto exporter = resource::ResourceExporter(*nrscResult);
        return exporter.exportAll({.outputDirectory = fs::path(std::getenv("HOME")) / "Downloads"});
    }
}

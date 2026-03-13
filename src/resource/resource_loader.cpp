//
// kiwakiwaaにより 2026/01/18 に作成されました。
//

#include "MKD/resource/resource_loader.hpp"

#include <algorithm>
#include <iostream>

namespace MKD
{
    ResourceLoader::ResourceLoader(const DictionaryPaths& paths)
        : paths_(paths)
    {
    }


    std::optional<ResourceStore> ResourceLoader::loadEntries(std::string_view contentDir, std::string_view dictId) const
    {
        return tryLoad<ResourceStore>(ResourceType::Contents, contentDir, dictId);
    }


    std::optional<NamedResourceStore> ResourceLoader::loadGraphics(std::string_view contentDir, std::string_view dictId) const
    {
        return tryLoad<NamedResourceStore>(ResourceType::Graphics, contentDir, dictId);
    }


    std::optional<std::variant<ResourceStore, NamedResourceStore>> ResourceLoader::loadAudio(std::string_view contentDir, std::string_view dictId) const
    {
        return tryLoadEither(ResourceType::Audio, contentDir, dictId);
    }


    std::vector<Keystore> ResourceLoader::loadKeystores(std::string_view contentDir, std::string_view dictId) const
    {
        return loadCollection<Keystore>(
            ResourceType::Keystores,
            contentDir,
            [](const auto& e) { return e.path().extension() == ".keystore"; },
            [&](const auto& e) { return Keystore::open(e.path(), std::string(dictId)); }
        );
    }


    std::vector<HeadlineStore> ResourceLoader::loadHeadlines(std::string_view contentDir) const
    {
        return loadCollection<HeadlineStore>(
            ResourceType::Headlines,
            contentDir,
            [](const auto& e) { return e.path().extension() == ".headlinestore"; },
            [&](const auto& e) { return HeadlineStore::load(e.path()); }
        );
    }


    std::vector<Font> ResourceLoader::loadFonts(std::string_view contentDir) const
    {
        return loadCollection<Font>(
            ResourceType::Fonts,
            contentDir,
            [](const auto& e) { return e.is_directory(); },
            [](const auto& e) { return Font::load(e.path()); }
        );
    }


    template<Openable T>
    std::optional<T> ResourceLoader::tryLoad(const ResourceType pathType, std::string_view contentDir, std::string_view dictId) const
    {
        const auto path = paths_.tryResolve(pathType, contentDir);
        if (!path)
            return std::nullopt;

        if (!hasResourceFiles(*path))
            return std::nullopt;

        auto result = [&]() {
            if constexpr (requires { T::open(*path, std::string(dictId)); })
                return T::open(*path, std::string(dictId));
            else
                return T::open(*path);
        }();

        return result ? std::optional<T>(std::move(*result)) : std::nullopt;
    }


    std::optional<std::variant<ResourceStore, NamedResourceStore>> ResourceLoader::tryLoadEither(const ResourceType pathType, std::string_view contentDir, std::string_view dictId) const
    {
        if (auto nrsc = tryLoad<NamedResourceStore>(pathType, contentDir, dictId))
            return std::variant<ResourceStore, NamedResourceStore>(std::move(*nrsc));

        if (auto rsc = tryLoad<ResourceStore>(pathType, contentDir, dictId))
            return std::variant<ResourceStore, NamedResourceStore>(std::move(*rsc));

        return std::nullopt;
    }


    template<typename T, typename Predicate, typename Loader>
    std::vector<T> ResourceLoader::loadCollection(const ResourceType type, std::string_view contentDir, Predicate pred, Loader loader) const
    {
        const auto path = paths_.tryResolve(type, contentDir);
        if (!path) return {};

        std::vector<T> results;
        for (const auto& entry : fs::directory_iterator(*path))
        {
            if (!pred(entry)) continue;
            if (auto result = loader(entry))
                results.push_back(std::move(*result));
        }
        return results;
    }


    bool ResourceLoader::hasResourceFiles(const fs::path& path)
    {
        return std::ranges::any_of(
            fs::directory_iterator(path),
            [](const auto& entry) {
                const auto& p = entry.path();
                return p.extension() == ".rsc" || p.extension() == ".nrsc";
            }
        );
    }
}

//
// kiwakiwaaにより 2026/01/18 に作成されました。
//

#include "monokakido/resource/resource_loader.hpp"

#include <iostream>

namespace monokakido
{
    ResourceLoader::ResourceLoader(const DictionaryPaths& paths)
        : paths_(paths)
    {
    }


    std::optional<Rsc> ResourceLoader::loadEntries(std::string_view dictId) const
    {
        return tryLoad<Rsc>(PathType::Contents, dictId);
    }


    std::optional<Nrsc> ResourceLoader::loadGraphics(std::string_view dictId) const
    {
        return tryLoad<Nrsc>(PathType::Graphics, dictId);
    }


    std::optional<std::variant<Rsc, Nrsc>> ResourceLoader::loadAudio(std::string_view dictId) const
    {
        return tryLoadEither(PathType::Audio, dictId);
    }


    std::vector<Font> ResourceLoader::loadFonts() const
    {
        const auto fontsPath = paths_.tryResolve(PathType::Fonts);
        if (!fontsPath)
            return {};

        // Iterate through subdirectories in the fonts folder
        std::vector<Font> fonts;
        for (const auto& entry : fs::directory_iterator(*fontsPath))
        {
            if (!entry.is_directory())
                continue;

            if (auto font = tryLoadResource<Rsc>(entry.path(), entry.path().filename().string()))
                fonts.emplace_back(entry.path().filename().string(), std::move(*font));
        }

        return fonts;
    }


    template<Openable T>
    std::optional<T> ResourceLoader::tryLoad(const PathType pathType, std::string_view dictId) const
    {
        const auto path = paths_.tryResolve(pathType);
        return path ? tryLoadResource<T>(*path, dictId) : std::nullopt;
    }


    std::optional<std::variant<Rsc, Nrsc> > ResourceLoader::tryLoadEither(const PathType pathType, std::string_view dictId) const
    {
        const auto path = paths_.tryResolve(pathType);
        if (!path)
            return std::nullopt;

        if (auto nrsc = tryLoadResource<Nrsc>(*path, dictId))
            return std::variant<Rsc, Nrsc>(std::move(*nrsc));

        if (auto rsc = tryLoadResource<Rsc>(*path, dictId))
            return std::variant<Rsc, Nrsc>(std::move(*rsc));

        return std::nullopt;
    }


    template<Openable T>
    std::optional<T> ResourceLoader::tryLoadResource(const fs::path& path, std::string_view dictId) const
    {
        if (!hasResourceFiles(path))
            return std::nullopt;

        auto result = [&]() {
            if constexpr (std::is_same_v<T, Rsc>)
                return T::open(path, dictId);
            else
                return T::open(path);
        }();

        if (!result)
            return std::nullopt;

        return std::move(*result);
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

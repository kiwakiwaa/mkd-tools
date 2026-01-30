//
// kiwakiwaaにより 2026/01/18 に作成されました。
//

#include "monokakido/resource/resource_loader.hpp"

#include <iostream>

namespace monokakido::resource
{
    ResourceLoader::ResourceLoader(const dictionary::DictionaryPaths& paths)
        : paths_(paths)
    {
    }


    std::optional<Nrsc> ResourceLoader::loadGraphics()
    {
        return tryLoad<Nrsc>(dictionary::PathType::Graphics, "graphics");
    }


    std::optional<Nrsc> ResourceLoader::loadAudio()
    {
        return tryLoad<Nrsc>(dictionary::PathType::Audio, "audio");
    }


    template<Openable T>
    std::optional<T> ResourceLoader::tryLoad(dictionary::PathType pathType, std::string_view resourceName)
    {
        const auto path = paths_.tryResolve(pathType);
        if (!path || !fs::exists(*path / "index.nidx"))
            return std::nullopt;

        auto result = T::open(*path);
        if (!result)
        {
            std::cerr << std::format("Failed to open {} at: {}\n{}",
                                     resourceName, path->string(), result.error());
            return std::nullopt;
        }

        return std::move(*result);
    }
}

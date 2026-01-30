//
// kiwakiwaaにより 2026/01/18 に作成されました。
//

#pragma once

#include "monokakido/dictionary/paths.hpp"
#include "monokakido/resource/nrsc/nrsc.hpp"


namespace monokakido::resource
{

    template<typename T>
    concept Openable = requires(const fs::path& path)
    {
        { T::open(path) } -> std::same_as<std::expected<T, std::string>>;
    };

    class ResourceLoader
    {
    public:

        explicit ResourceLoader(const dictionary::DictionaryPaths& paths);

        [[nodiscard]] std::optional<Nrsc> loadGraphics();
        [[nodiscard]] std::optional<Nrsc> loadAudio();

        // [[nodiscard]] std::optional<resource::Rsc> loadEntries();

    private:

        const dictionary::DictionaryPaths& paths_;

        template<Openable T>
        std::optional<T> tryLoad(dictionary::PathType pathType, std::string_view resourceName);
    };



}
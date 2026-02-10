//
// kiwakiwaaにより 2026/01/18 に作成されました。
//

#pragma once

#include "monokakido/dictionary/paths.hpp"
#include "monokakido/resource/nrsc/nrsc.hpp"
#include "monokakido/resource/rsc/rsc.hpp"
#include "monokakido/resource/font.hpp"

#include <variant>

namespace monokakido
{

    template<typename T>
    concept Openable = requires(const fs::path& path) {
        { T::open(path) } -> std::same_as<std::expected<T, std::string>>;
    } || requires(const fs::path& path, const std::string& dictId) {
        { T::open(path, dictId) } -> std::same_as<std::expected<T, std::string>>;
    };

    class ResourceLoader
    {
    public:

        explicit ResourceLoader(const DictionaryPaths& paths);

        [[nodiscard]] std::optional<Rsc> loadEntries(std::string_view dictId) const;
        [[nodiscard]] std::optional<Nrsc> loadGraphics(std::string_view dictId) const;
        [[nodiscard]] std::optional<std::variant<Rsc, Nrsc>> loadAudio(std::string_view dictId) const;
        [[nodiscard]] std::vector<Font> loadFonts() const;

    private:

        const DictionaryPaths& paths_;

        template<Openable T>
        std::optional<T> tryLoad(PathType pathType, std::string_view dictId) const;

        [[nodiscard]] std::optional<std::variant<Rsc, Nrsc>> tryLoadEither(PathType pathType, std::string_view dictId) const;

        template<Openable T>
        std::optional<T> tryLoadResource(const fs::path& path, std::string_view dictId) const;

        static bool hasResourceFiles(const fs::path& path) ;
    };



}
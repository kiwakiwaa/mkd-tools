//
// kiwakiwaaにより 2026/01/18 に作成されました。
//

#pragma once

#include "MKD/dictionary/paths.hpp"
#include "MKD/resource/nrsc/nrsc.hpp"
#include "MKD/resource/rsc/rsc.hpp"
#include "MKD/resource/font.hpp"
#include "MKD/resource/keystore/keystore.hpp"
#include "MKD/resource/headline/headline_store.hpp"

#include <variant>

namespace MKD
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
        [[nodiscard]] std::vector<Keystore> loadKeystores(std::string_view dictId) const;
        [[nodiscard]] std::vector<HeadlineStore> loadHeadlines() const;
        [[nodiscard]] std::vector<Font> loadFonts() const;

    private:

        const DictionaryPaths& paths_;

        template<Openable T>
        std::optional<T> tryLoad(ResourceType pathType, std::string_view dictId) const;

        template<typename T, typename Predicate, typename Loader>
        std::vector<T> loadCollection(ResourceType type, Predicate pred, Loader loader) const;

        [[nodiscard]] std::optional<std::variant<Rsc, Nrsc>> tryLoadEither(ResourceType pathType, std::string_view dictId) const;

        static bool hasResourceFiles(const fs::path& path);
    };
}
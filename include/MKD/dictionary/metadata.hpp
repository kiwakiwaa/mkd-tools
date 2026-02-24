//
// kiwakiwaaにより 2026/01/14 に作成されました。
//

#pragma once

#include <glaze/glaze.hpp>

#include <filesystem>
#include <optional>
#include <string>

namespace fs = std::filesystem;

namespace MKD
{
    struct LocalizedString
    {
        std::optional<std::string> en;
        std::optional<std::string> ja;
    };

    struct DictionaryContent
    {
        std::string identifier;
        std::string directory;
        std::optional<std::string> version;
        std::optional<LocalizedString> title;
        std::optional<LocalizedString> publisher;
        std::optional<LocalizedString> edition;
        std::optional<std::vector<std::string> > languages;
    };

    struct DictionaryMetadata
    {
        static std::expected<DictionaryMetadata, std::string> loadFromPath(const fs::path& path);

        std::optional<LocalizedString> displayName;
        std::optional<LocalizedString> description;
        std::optional<LocalizedString> genre;
        std::optional<LocalizedString> productTitle;
        std::string productIdentifier;
        std::optional<std::string> category;
        std::optional<std::string> version;
        std::vector<DictionaryContent> contents;
    };
}

template<>
struct glz::meta<MKD::LocalizedString>
{
    using T = MKD::LocalizedString;
    static constexpr auto value = object(
        "en", &T::en,
        "ja", &T::ja
    );
};

template<>
struct glz::meta<MKD::DictionaryContent>
{
    using T = MKD::DictionaryContent;
    static constexpr auto value = object(
        "DSContentIdentifier", &T::identifier,
        "DSContentDirectory", &T::directory,
        "DSContentVersion", &T::version,
        "DSContentTitle", &T::title,
        "DSContentPublisher", &T::publisher,
        "DSContentEdition", &T::edition,
        "DSContentLanguages", &T::languages
    );
};

template<>
struct glz::meta<MKD::DictionaryMetadata>
{
    using T = MKD::DictionaryMetadata;
    static constexpr auto value = object(
        "DSProductDisplayName", &T::displayName,
        "DSProductDescription", &T::description,
        "DSProductGenre", &T::genre,
        "DSProductTitle", &T::productTitle,
        "DSProductIdentifier", &T::productIdentifier,
        "DSProductCategory", &T::category,
        "DSProductVersion", &T::version,
        "DSProductContents", &T::contents
    );
};

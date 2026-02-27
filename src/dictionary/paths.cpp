//
// kiwakiwaaにより 2026/01/15 に作成されました。
//

#include "MKD/dictionary/paths.hpp"


using namespace std::literals::string_view_literals;

namespace MKD
{
    namespace detail
    {
        std::optional<fs::path> findResourcePath(
            const fs::path& contentDir,
            std::span<const std::string_view> candidateNames)
        {
            for (const auto& name : candidateNames)
            {
                if (fs::path path = contentDir / name; fs::exists(path / "index.nidx"))
                    return std::move(path);
            }
            return std::nullopt;
        }
    }


    std::expected<DictionaryPaths, std::string> DictionaryPaths::create(fs::path productRoot)
    {
        auto contentsRoot = productRoot / "Contents";
        if (!fs::is_directory(contentsRoot))
            return std::unexpected("Missing Contents directory: " + contentsRoot.string());

        return DictionaryPaths(std::move(productRoot), std::move(contentsRoot));
    }


    DictionaryPaths::DictionaryPaths(fs::path productRoot, fs::path contentsRoot)
        : productRoot_(std::move(productRoot))
          , contentsRoot_(std::move(contentsRoot))
    {
    }


    std::optional<fs::path> DictionaryPaths::tryResolve(
        const ResourceType type, std::string_view contentDir) const
    {
        const auto base = contentsRoot_ / contentDir;

        switch (type)
        {
            case ResourceType::Entries: return existingDir(base / "contents");
            case ResourceType::Audio: return existingDir(base / "audio");
            case ResourceType::Graphics:
            {
                static constexpr std::array candidates = {"graphics"sv, "img"sv};
                if (auto path = detail::findResourcePath(base, candidates))
                    return *path;
                return std::nullopt;
            }
            case ResourceType::Keystores: return existingDir(base / "key");
            case ResourceType::Headlines: return existingDir(base / "headline");
            case ResourceType::Fonts: return existingDir(base / "fonts");
        }
        std::unreachable();
    }


    std::optional<fs::path> DictionaryPaths::existingDir(const fs::path& path)
    {
        return fs::is_directory(path) ? std::optional(path) : std::nullopt;
    }
}

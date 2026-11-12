//
// kiwakiwaaにより 2026/01/15 に作成されました。
//

#include "MKD/dictionary/paths.hpp"
#include "paths.hpp"
#include "io/write_validation.hpp"
#include "dictionary/package/package_format.hpp"

#include "MKD/dictionary/content_analysis.hpp"
#include <charconv>
#include <array>
#include <span>

namespace MKD
{
    Result<DictionaryPaths> DictionaryPaths::create(fs::path productRoot)
    {
        auto contentsRoot = productRoot / detail::monokakido::paths::productContents;
        if (!fs::is_directory(contentsRoot))
            return std::unexpected("Missing Contents directory: " + contentsRoot.string());

        return DictionaryPaths(std::move(productRoot), std::move(contentsRoot));
    }


    DictionaryPaths::DictionaryPaths(fs::path productRoot, fs::path contentsRoot)
        : productRoot_(std::move(productRoot))
          , contentsRoot_(std::move(contentsRoot))
    {
    }


    Result<std::optional<fs::path>> DictionaryPaths::resolve(
        const ResourceType type, std::string_view contentDir) const
    {
        const auto base = contentsRoot_ / contentDir;

        switch (type)
        {
        case ResourceType::Contents:
            return existingDir(base / detail::monokakido::paths::contents);
        case ResourceType::AppendixEntries:
            return existingDir(base);
        case ResourceType::Audio:
        case ResourceType::AudioLegacy:
            return existingDir(base / detail::monokakido::paths::audio);
        case ResourceType::Graphics:
            {
                static constexpr std::array candidates{
                    std::string_view(detail::monokakido::paths::graphics),
                    std::string_view(detail::monokakido::paths::legacyGraphics)
                };
                for (const auto name : candidates)
                {
                    auto path = existingDir(base / name);
                    if (!path || *path)
                        return path;
                }
                return std::optional<fs::path>{};
            }
        case ResourceType::Keystores:
            return existingDir(base / detail::monokakido::paths::keys);
        case ResourceType::Headlines:
            return existingDir(base / detail::monokakido::paths::headlines);
        case ResourceType::Fonts:
            return existingDir(base / detail::monokakido::paths::fonts);
        }
        std::unreachable();
    }


    Result<std::optional<fs::path>> DictionaryPaths::existingDir(const fs::path& path)
    {
        std::error_code ec;
        const auto status = fs::status(path, ec);
        if (ec == std::errc::no_such_file_or_directory)
            return std::optional<fs::path>{};

        if (ec)
            return std::unexpected("Cannot inspect '" + path.string() + "': " + ec.message());

        if (!fs::exists(status))
            return std::optional<fs::path>{};

        if (!fs::is_directory(status))
            return std::unexpected("Resource path is not a directory: " + path.string());

        return std::optional(path);
    }
}

namespace MKD::detail
{
    Result<std::filesystem::path> resolveProductPath(const std::filesystem::path& contents, std::string_view path)
    {
        if (auto valid = WriteValidation::relativePath(path); !valid)
            return std::unexpected(valid.error());

        if (path.find_first_of("?#") != std::string_view::npos)
            return std::unexpected("Appendix paths must not contain query strings or fragments");

        std::error_code error;
        auto resolved = std::filesystem::weakly_canonical(contents / WriteValidation::fromUtf8(path), error);
        if (error)
            return std::unexpected(error.message());

        const auto base = std::filesystem::weakly_canonical(contents, error);
        if (error)
            return std::unexpected(error.message());

        const auto relative = resolved.lexically_relative(base);
        if (relative.empty() || *relative.begin() == "..")
            return std::unexpected("Appendix path escapes the product");

        return resolved;
    }

    Result<std::string> decodeUrlPath(std::string_view value)
    {
        std::string decoded;
        for (size_t i = 0; i < value.size(); ++i)
        {
            if (value[i] != '%')
            {
                decoded += value[i];
                continue;
            }

            unsigned byte{};
            if (i + 2 >= value.size())
                return std::unexpected("Incomplete URL percent escape");

            const auto parsed = std::from_chars(value.data() + i + 1, value.data() + i + 3, byte, 16);
            if (parsed.ec != std::errc{} || parsed.ptr != value.data() + i + 3)
                return std::unexpected("Invalid URL percent escape");

            decoded += static_cast<char>(byte);
            i += 2;
        }
        if (auto valid = WriteValidation::text(decoded, true); !valid)
            return std::unexpected(valid.error());
        return decoded;
    }

    Result<std::string> resolveDocumentPath(std::string_view value, std::string_view sourcePath)
    {
        auto decoded = decodeUrlPath(value.substr(0, value.find('?')));
        if (!decoded)
            return std::unexpected(decoded.error());

        const auto base = WriteValidation::fromUtf8(sourcePath).parent_path();
        const auto path = (base / WriteValidation::fromUtf8(*decoded)).lexically_normal().generic_string();
        if (auto valid = WriteValidation::relativePath(path); !valid)
            return std::unexpected(valid.error());

        return path;
    }

    Result<std::optional<std::string>> resolveAssetReference(const content::AssetReference& reference,
                                                             std::string_view sourcePath)
    {
        if (reference.syntax == content::ResourceSyntax::Path)
            return reference.path;

        auto value = std::string_view(reference.path);
        if (value.empty() || value.front() == '#' || value.contains(':') || value.starts_with("//"))
            return std::optional<std::string>{};

        value = value.substr(0, value.find_first_of("?#"));
        if (reference.syntax != content::ResourceSyntax::CssUrl)
        {
            if (auto valid = WriteValidation::relativePath(value); !valid)
                return std::unexpected(valid.error());
            return std::string(value);
        }

        auto decoded = decodeUrlPath(value);
        if (!decoded)
            return std::unexpected("Invalid percent escape in CSS resource path");

        const auto path = (WriteValidation::fromUtf8(sourcePath).parent_path() / WriteValidation::fromUtf8(*decoded))
                          .lexically_normal()
                          .generic_u8string();
        const std::string utf8(reinterpret_cast<const char*>(path.data()), path.size());
        if (auto valid = WriteValidation::relativePath(utf8); !valid)
            return std::unexpected(valid.error());

        return utf8;
    }
}

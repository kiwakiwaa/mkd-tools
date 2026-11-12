//
// kiwakiwaaにより 2026/10/24 に作成されました。
//

#include "asset_writer.hpp"
#include "dictionary/package/package_format.hpp"
#include "dictionary/content/content_markup.hpp"
#include "io/write_validation.hpp"
#include "output/directory_output.hpp"
#include <algorithm>
#include <array>
#include <pugixml.hpp>

namespace MKD::detail
{
    Result<void> AssetWriter::validateAssetPath(std::string_view relative)
    {
        if (auto valid = WriteValidation::relativePath(relative); !valid)
            return valid;

        const auto path = WriteValidation::fromUtf8(relative);
        const auto first = *path.begin();

        if (first == monokakido::paths::contents || first == monokakido::paths::keys || first ==
            monokakido::paths::secondaryKeys ||
            first == monokakido::paths::headlines || first == monokakido::paths::audio || first ==
            monokakido::paths::graphics ||
            first == monokakido::paths::fonts || first == monokakido::paths::contentInfo)
            return std::unexpected("Asset path is reserved for generated dictionary resources");

        return {};
    }

    Result<void> AssetWriter::writeAsset(std::string_view relative, std::span<const uint8_t> bytes)
    {
        if (auto valid = validateAssetPath(relative); !valid)
            return valid;

        const auto path = WriteValidation::fromUtf8(relative);
        if (!assets_.emplace(relative).second)
            return std::unexpected("Duplicate dictionary asset");

        DirectoryOutput directory(root_, false, false);
        if (auto prepared = directory.prepare(); !prepared)
            return prepared;

        auto written = directory.writeFile(path, [bytes](OutputFile& file) { return file.write(bytes); });
        if (!written)
            return std::unexpected(written.error());

        if (written->skipped)
            return std::unexpected("Duplicate dictionary asset");

        return {};
    }

    Result<void> AssetWriter::writeMedia(std::string_view name, std::span<const uint8_t> bytes, MediaKind kind)
    {
        const bool isAudio = kind == MediaKind::Audio;
        if (auto valid = WriteValidation::relativePath(name); !valid)
            return valid;

        if (name.contains('/'))
            return std::unexpected("Packed media name must be a single path component");

        auto& writer = isAudio ? audio_ : graphics_;
        if (!writer)
        {
            auto created =
                NamedResourceStoreWriter::create(
                    root_ / (isAudio ? monokakido::paths::audio : monokakido::paths::graphics));
            if (!created)
                return std::unexpected(created.error());

            writer = std::move(*created);
        }

        if (auto written = writer->add(name, bytes); !written)
            return written;

        (isAudio ? audioNames_ : graphicNames_).emplace(name);
        return {};
    }

    Result<void> AssetWriter::validateGaiji(std::string_view path, std::span<const uint8_t> bytes)
    {
        if (auto valid = ContentMarkup::gaijiPath(path); !valid)
            return valid;

        if (path.ends_with(".svg"))
        {
            pugi::xml_document document;
            if (!document.load_buffer(bytes.data(), bytes.size(), pugi::parse_default, pugi::encoding_utf8) ||
                std::string_view(document.document_element().name()) != "svg")
                return std::unexpected("Gaiji SVG must be a well-formed SVG document");

            for (const auto node : document.children())
                if (node.type() == pugi::node_element && node != document.document_element())
                    return std::unexpected("Gaiji SVG must contain exactly one document element");
        }
        else
        {
            constexpr std::array<uint8_t, 8> signature{137, 80, 78, 71, 13, 10, 26, 10};
            if (bytes.size() < signature.size() || !std::ranges::equal(bytes.first(signature.size()), signature))
                return std::unexpected("Gaiji PNG has an invalid signature");
        }
        return {};
    }

    std::set<std::string, std::less<>> AssetWriter::resourcePaths() const
    {
        auto paths = assets_;
        for (const auto& name : graphicNames_)
            paths.insert(std::string(monokakido::paths::graphics) + "/" + name);

        for (const auto& name : audioNames_)
            paths.insert(std::string(monokakido::paths::audio) + "/" + name);

        return paths;
    }

    bool AssetWriter::contains(std::string_view path) const
    {
        return containsLoose(path) || (path.starts_with("graphics/") && graphicNames_.contains(path.substr(9))) ||
            (path.starts_with("audio/") && audioNames_.contains(path.substr(6)));
    }

    Result<void> AssetWriter::finish() const
    {
        if (graphics_)
            if (auto result = graphics_->finish(); !result)
                return result;

        if (audio_)
            if (auto result = audio_->finish(); !result)
                return result;

        return {};
    }
}

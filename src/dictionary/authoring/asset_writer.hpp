//
// kiwakiwaaにより 2026/10/24 に作成されました。
//

#pragma once

#include "MKD/resource/named_resource_store_writer.hpp"
#include <set>

namespace MKD::detail
{
    enum class MediaKind
    {
        Graphic,
        Audio
    };

    class AssetWriter
    {
    public:
        explicit AssetWriter(std::filesystem::path root) : root_(std::move(root))
        {
        }

        static Result<void> validateAssetPath(std::string_view relative);
        static Result<void> validateGaiji(std::string_view path, std::span<const uint8_t> bytes);
        Result<void> writeAsset(std::string_view relative, std::span<const uint8_t> bytes);
        Result<void> writeMedia(std::string_view name, std::span<const uint8_t> bytes, MediaKind kind);
        Result<void> finish() const;
        void registerLoose(std::string path) { assets_.insert(std::move(path)); }
        [[nodiscard]] bool contains(std::string_view path) const;
        [[nodiscard]] bool containsLoose(std::string_view path) const { return assets_.contains(path); }
        std::set<std::string, std::less<>> resourcePaths() const;
        [[nodiscard]] size_t audioCount() const { return audioNames_.size(); }

    private:
        std::filesystem::path root_;
        std::optional<NamedResourceStoreWriter> graphics_, audio_;
        std::set<std::string, std::less<>> assets_, graphicNames_, audioNames_;
    };
}

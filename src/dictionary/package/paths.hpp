//
// kiwakiwaaにより 2026/10/24 に作成されました。
//

#pragma once
#include "MKD/result.hpp"
#include <filesystem>
#include <optional>

namespace MKD::content
{
    struct AssetReference;
}

namespace MKD::detail
{
    Result<std::string> decodeUrlPath(std::string_view value);
    Result<std::string> resolveDocumentPath(std::string_view value, std::string_view sourcePath);
    Result<std::optional<std::string>> resolveAssetReference(const content::AssetReference& reference,
                                                             std::string_view sourcePath);
    Result<std::filesystem::path> resolveProductPath(const std::filesystem::path& contents, std::string_view path);
}

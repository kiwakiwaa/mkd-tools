//
// kiwakiwaaにより 2026/02/19 に作成されました。
//

#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace fs = std::filesystem;

namespace MKD::macOS
{
    [[nodiscard]] std::filesystem::path getContainerPathByGroupIdentifier(const std::string& groupIdentifier);

    [[nodiscard]] std::optional<fs::path> getMonokakidoDictionariesPath(const std::string& groupIdentifier, const std::string& relativePath);
}

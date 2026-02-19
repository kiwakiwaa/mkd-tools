//
// kiwakiwaaにより 2026/02/19 に作成されました。
//

#pragma once

#include <filesystem>
#include <string>

namespace monokakido::macos
{
    [[nodiscard]] std::filesystem::path getContainerPathByGroupIdentifier(const std::string& groupIdentifier);
}

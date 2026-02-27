//
// kiwakiwaaにより 2026/02/27 に作成されました。
//

#pragma once

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace MKD::macOS
{
    std::optional<std::vector<uint8_t>> loadSavedBookmark(std::string_view key = "MKDBookmark");

    void saveBookmark(const std::vector<uint8_t>& bookmarkData, std::string_view key = "MKDBookmark");

    void clearSavedBookmark(std::string_view key = "MKDBookmark");
}
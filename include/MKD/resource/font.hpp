//
// kiwakiwaaにより 2026/02/01 に作成されました。
//

#pragma once

#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <span>
#include <vector>

namespace fs = std::filesystem;

namespace MKD
{
    class Font
    {
    public:
        static std::expected<Font, std::string> load(const fs::path& directoryPath);

        [[nodiscard]] const std::string& name() const noexcept;

        [[nodiscard]] std::expected<std::span<const uint8_t>, std::string> getData() const;

        [[nodiscard]] std::optional<std::string> detectType() const;

        [[nodiscard]] bool isEmpty() const noexcept;

    private:
        explicit Font(std::string name, std::vector<uint8_t> data);

        std::string name_;
        std::vector<uint8_t> data_;
    };
}

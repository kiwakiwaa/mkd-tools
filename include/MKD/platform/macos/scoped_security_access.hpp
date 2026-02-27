//
// kiwakiwaaにより 2026/01/15 に作成されました。
//

#pragma once

#include <expected>
#include <filesystem>
#include <optional>
#include <vector>

namespace fs = std::filesystem;

namespace MKD::macOS
{
    struct BookmarkData
    {
        std::vector<uint8_t> data;
        fs::path resolvedPath;
    };

    class ScopedSecurityAccess
    {
    public:

        ScopedSecurityAccess() = default;
        explicit ScopedSecurityAccess(const fs::path& path);
        ~ScopedSecurityAccess();

        ScopedSecurityAccess(const ScopedSecurityAccess&) = delete;
        ScopedSecurityAccess& operator=(const ScopedSecurityAccess&) = delete;

        ScopedSecurityAccess(ScopedSecurityAccess&&) noexcept;
        ScopedSecurityAccess& operator=(ScopedSecurityAccess&&) noexcept;

        [[nodiscard]] bool isValid() const;

    private:
        void release() noexcept;

        void* url_ = nullptr; // NSUrl*

    };

    struct BookmarkAccess
    {
        fs::path path;
        ScopedSecurityAccess access;
    };

    std::expected<BookmarkAccess, std::string> restoreAccessFromBookmark(const std::vector<uint8_t>& bookmarkData);

    std::optional<BookmarkData> createSecurityScopedBookmark(const fs::path& path);
}
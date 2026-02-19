//
// kiwakiwaaにより 2026/01/15 に作成されました。
//

#pragma once

#include <expected>
#include <filesystem>
#include <optional>
#include <vector>

namespace monokakido::macos
{
    struct BookmarkData
    {
        std::vector<uint8_t> data;
        std::filesystem::path resolvedPath;
    };

    class ScopedSecurityAccess
    {
    public:

        ScopedSecurityAccess() = default;
        explicit ScopedSecurityAccess(const std::filesystem::path& path);
        ~ScopedSecurityAccess();

        // not copyable
        ScopedSecurityAccess(const ScopedSecurityAccess&) = delete;
        ScopedSecurityAccess& operator=(const ScopedSecurityAccess&) = delete;

        // movable
        ScopedSecurityAccess(ScopedSecurityAccess&&) noexcept;
        ScopedSecurityAccess& operator=(ScopedSecurityAccess&&) noexcept;

        [[nodiscard]] bool isValid() const;

    private:

        void release() noexcept;

        void* url_ = nullptr; // NSUrl*

    };

    struct BookmarkAccess
    {
        std::filesystem::path path;
        ScopedSecurityAccess access;
    };

    std::expected<BookmarkAccess, std::string> restoreAccessFromBookmark(const std::vector<uint8_t>& bookmarkData);

    std::optional<BookmarkData> promptForDictionariesAccess();

    std::optional<std::vector<uint8_t>> loadSavedBookmark();

    void saveBookmark(const std::vector<uint8_t>& bookmarkData);

    void clearSavedBookmark();

}
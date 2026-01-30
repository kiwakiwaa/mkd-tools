//
// kiwakiwaaにより 2026/01/14 に作成されました。
//

#pragma once

#include "monokakido/core/platform/scoped_security_access.hpp"

#include <expected>
#include <filesystem>
#include <optional>
#include <vector>

namespace monokakido::dictionary
{
    static constexpr auto MONOKAKIDO_GROUP_ID = "group.jp.monokakido.Dictionaries";
    static constexpr auto DICTIONARIES_PATH = "Library/Application Support/com.dictionarystore/dictionaries";

    namespace fs = std::filesystem;

    struct DictionaryInfo
    {
        std::string id;
        std::filesystem::path path;
    };


    class DictionaryCatalog
    {
    public:

        static DictionaryCatalog& instance();

        DictionaryCatalog(const DictionaryCatalog&) = delete;
        DictionaryCatalog& operator=(const DictionaryCatalog&) = delete;

        enum class AccessStatus
        {
            Granted,
            NeedsPermission,
            PermissionDenied
        };

        AccessStatus checkAccess() const;
        bool requestAccess() const;

        std::expected<std::vector<DictionaryInfo>, std::string> findAllAvailable() const;
        [[nodiscard]] std::expected<DictionaryInfo, std::string> findById(std::string_view id) const;

        void refresh() const;

    private:

        DictionaryCatalog() = default;

        mutable std::optional<std::vector<DictionaryInfo>> cachedDictionaries_;
        mutable std::optional<fs::path> authorizedPath_;
        mutable std::optional<platform::fs::ScopedSecurityAccess> securityAccess_;

    };
}
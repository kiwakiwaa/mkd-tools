//
// kiwakiwaaにより 2026/02/19 に作成されました。
//

#include "MKD/platform/macos/macos_dictionary_source.hpp"
#include "bookmark_store.hpp"
#include "folder_prompt.hpp"
#include "fs.hpp"
#include "scoped_security_access.hpp"

#include <unistd.h>
#include <sys/fcntl.h>
#include <format>
#include <optional>

namespace MKD
{
    struct MacOSDictionarySource::Impl
    {
        mutable std::optional<std::vector<DictionaryInfo>> cachedDictionaries_;
        mutable std::optional<fs::path> authorizedPath_;
        mutable macOS::ScopedSecurityAccess securityAccess_;
    };


    MacOSDictionarySource::MacOSDictionarySource()
        : impl_(std::make_unique<Impl>())
    {
    }


    MacOSDictionarySource::~MacOSDictionarySource() = default;
    MacOSDictionarySource::MacOSDictionarySource(MacOSDictionarySource&&) noexcept = default;
    MacOSDictionarySource& MacOSDictionarySource::operator=(MacOSDictionarySource&&) noexcept = default;


    Result<std::vector<DictionaryInfo>> MacOSDictionarySource::findAllAvailable() const
    {
        if (!checkAccess())
            return std::unexpected("Access not granted. Call requestAccess() first.");

        if (!impl_->authorizedPath_)
            return std::unexpected("No authorized path available");

        std::vector<DictionaryInfo> result;

        try
        {
            for (const auto& entry : std::filesystem::directory_iterator(impl_->authorizedPath_.value()))
            {
                if (!entry.is_directory()) continue;
                result.emplace_back(entry.path().stem(), entry.path());
            }
        }
        catch (const std::filesystem::filesystem_error& e)
        {
            return std::unexpected(std::format("Failed to read dictionaries: {}", e.what()));
        }

        impl_->cachedDictionaries_ = result;
        return result;
    }


    Result<DictionaryInfo> MacOSDictionarySource::findById(std::string_view dictId) const
    {
        if (!impl_->cachedDictionaries_)
        {
            if (auto all = findAllAvailable(); !all) return std::unexpected(all.error());
        }

        for (const auto& info : *impl_->cachedDictionaries_)
        {
            if (info.id == dictId) return info;
        }

        return std::unexpected(std::format("Dictionary '{}' not found", dictId));
    }


    bool MacOSDictionarySource::checkAccess() const
    {
        if (impl_->securityAccess_.isValid())
            return true;

        if (canAccessDirectly())
            return true;

        return tryRestoreFromBookmark();
    }


    bool MacOSDictionarySource::requestAccess(const bool activateProcess) const
    {
        if (!isMonokakidoInstalled())
            return false;

        if (activateProcess)
            macOS::activateAsAccessory();

        const auto dictPath = macOS::getMonokakidoDictionariesPath(
            MONOKAKIDO_GROUP_ID, DICTIONARIES_SUBPATH);

        auto selectedPath = macOS::promptForFolder({
            .message = "Please select the Monokakido Dictionaries folder to grant access",
            .confirmButton = "Grant Access",
            .initialDirectory = dictPath,
        });

        if (!selectedPath) return false;

        auto bookmarkData = macOS::createSecurityScopedBookmark(*selectedPath);
        if (!bookmarkData) return false;

        macOS::saveBookmark(bookmarkData->data);
        impl_->securityAccess_ = macOS::ScopedSecurityAccess(bookmarkData->resolvedPath);

        if (!impl_->securityAccess_.isValid())
            return false;

        impl_->authorizedPath_ = bookmarkData->resolvedPath;
        impl_->cachedDictionaries_.reset();
        return true;
    }


    bool MacOSDictionarySource::isMonokakidoInstalled()
    {
        return macOS::getMonokakidoDictionariesPath(MONOKAKIDO_GROUP_ID, DICTIONARIES_SUBPATH).has_value();
    }


    bool MacOSDictionarySource::tryRestoreFromBookmark() const
    {
        const auto bookmark = macOS::loadSavedBookmark();
        if (!bookmark) return false;

        auto access = macOS::restoreAccessFromBookmark(*bookmark);
        if (!access) return false;

        impl_->authorizedPath_ = std::move(access->path);
        impl_->securityAccess_ = std::move(access->access);
        return true;
    }


    bool MacOSDictionarySource::canAccessDirectly() const
    {
        const auto dictPath = macOS::getMonokakidoDictionariesPath(MONOKAKIDO_GROUP_ID, DICTIONARIES_SUBPATH);
        if (!dictPath) return false;

        const fs::path& path = *dictPath;

        if (std::error_code ec; !fs::exists(path, ec) || ec)
            return false;

        const int fd = ::open(path.c_str(), O_RDONLY | O_DIRECTORY);
        if (fd < 0)
            return false;

        ::close(fd);

        impl_->authorizedPath_ = path;
        return true;
    }

}

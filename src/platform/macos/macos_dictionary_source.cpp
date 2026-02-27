//
// kiwakiwaaにより 2026/02/19 に作成されました。
//

#include "MKD/platform/macos/macos_dictionary_source.hpp"
#include "MKD/platform/macos/bookmark_store.hpp"
#include "MKD/platform/macos/folder_prompt.hpp"
#include "MKD/platform/macos/fs.hpp"

#include <format>

namespace MKD
{
    std::expected<std::vector<DictionaryInfo>, std::string> MacOSDictionarySource::findAllAvailable() const
    {
        if (!checkAccess())
            return std::unexpected("Access not granted. Call requestAccess() first.");

        if (!authorizedPath_)
            return std::unexpected("No authorized path available");

        std::vector<DictionaryInfo> result;

        try
        {
            for (const auto& entry : std::filesystem::directory_iterator(*authorizedPath_))
            {
                if (!entry.is_directory()) continue;
                result.emplace_back(entry.path().stem(), entry.path());
            }
        }
        catch (const std::filesystem::filesystem_error& e)
        {
            return std::unexpected(std::format("Failed to read dictionaries: {}", e.what()));
        }

        cachedDictionaries_ = result;
        return result;
    }


    std::expected<DictionaryInfo, std::string> MacOSDictionarySource::findById(std::string_view dictId) const
    {
        if (!cachedDictionaries_)
        {
            if (auto all = findAllAvailable(); !all) return std::unexpected(all.error());
        }

        for (const auto& info : *cachedDictionaries_)
        {
            if (info.id == dictId) return info;
        }

        return std::unexpected(std::format("Dictionary '{}' not found", dictId));
    }


    bool MacOSDictionarySource::checkAccess() const
    {
        if (securityAccess_.isValid())
            return true;

        return tryRestoreFromBookmark();
    }


    bool MacOSDictionarySource::requestAccess(const bool activateProcess) const
    {
        if (!isMonokakidoInstalled())
            return false;

        if (activateProcess)
            macOS::activateAsAccessory();

        auto dictPath = macOS::getMonokakidoDictionariesPath(MONOKAKIDO_GROUP_ID, DICTIONARIES_SUBPATH);

        auto selectedPath = macOS::promptForFolder({
            .message = "Please select the Monokakido Dictionaries folder to grant access",
            .confirmButton = "Grant Access",
            .initialDirectory = dictPath,
        });

        if (!selectedPath) return false;

        auto bookmarkData = macOS::createSecurityScopedBookmark(*selectedPath);
        if (!bookmarkData) return false;

        macOS::saveBookmark(bookmarkData->data);
        securityAccess_ = macOS::ScopedSecurityAccess(bookmarkData->resolvedPath);

        if (!securityAccess_.isValid())
            return false;

        authorizedPath_ = bookmarkData->resolvedPath;
        cachedDictionaries_.reset();
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

        authorizedPath_ = std::move(access->path);
        securityAccess_ = std::move(access->access);
        return true;
    }
}

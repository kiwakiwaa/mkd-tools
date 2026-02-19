//
// kiwakiwaaにより 2026/02/19 に作成されました。
//

#include "monokakido/platform/macos/macos_dictionary_source.hpp"
#include "monokakido/platform/macos/fs.hpp"

#include <format>

namespace monokakido
{
    std::expected<std::vector<DictionaryInfo>, std::string> MacOSDictionarySource::findAllAvailable() const
    {
        if (const auto status = checkAccess(); status == AccessStatus::NeedsPermission)
            return std::unexpected("Access to dictionaries folder not granted. Please call requestAccess().");

        if (!authorizedPath_)
            return std::unexpected("No authorized path available");

        std::vector<DictionaryInfo> result;
        const auto containerPath = macos::getContainerPathByGroupIdentifier(MONOKAKIDO_GROUP_ID);
        if (containerPath.empty())
            return result;

        try
        {
            for (const auto path = containerPath / DICTIONARIES_PATH; const auto& folder : fs::directory_iterator(path))
            {
                if (!folder.is_directory())
                    continue;

                const auto info = DictionaryInfo(folder.path().stem(), folder.path());
                result.emplace_back(info);
            }
        }
        catch (const fs::filesystem_error& e)
        {
            return std::unexpected(std::format("Failed to read dictionaries: {}", e.what()));
        }

        cachedDictionaries_ = result;
        return result;
    }


    std::expected<DictionaryInfo, std::string> MacOSDictionarySource::findById(std::string_view dictId) const
    {
        if (const auto status = checkAccess(); status == AccessStatus::NeedsPermission)
            return std::unexpected("Access to dictionaries folder not granted. Please call requestAccess().");

        if (!authorizedPath_)
            return std::unexpected("No authorized path available");

        if (!cachedDictionaries_)
            findAllAvailable();

        for (const auto& info : cachedDictionaries_.value())
        {
            if (info.id == dictId)
                return info;
        }

        return std::unexpected(std::format("Failed to find dictionary with id '{}'", dictId));
    }


    MacOSDictionarySource::AccessStatus MacOSDictionarySource::checkAccess() const
    {
        if (securityAccess_.isValid())
            return AccessStatus::Granted;

        // Attempt to restore access from saved bookmark
        if (const auto bookmark = macos::loadSavedBookmark())
        {
            if (auto access = macos::restoreAccessFromBookmark(*bookmark))
            {
                authorizedPath_ = access->path;
                securityAccess_ = std::move(access->access);
                return AccessStatus::Granted;
            }
        }

        return AccessStatus::NeedsPermission;
    }


    bool MacOSDictionarySource::requestAccess() const
    {
        auto bookmarkData = macos::promptForDictionariesAccess();
        if (!bookmarkData)
            return false;

        macos::saveBookmark(bookmarkData->data);

        authorizedPath_ = bookmarkData->resolvedPath;
        securityAccess_ = macos::ScopedSecurityAccess(bookmarkData->resolvedPath);

        if (!securityAccess_.isValid())
        {
            authorizedPath_.reset();
            return false;
        }

        // Clear cache to force refresh
        cachedDictionaries_.reset();
        return true;
    }
}

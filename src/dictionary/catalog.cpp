//
// kiwakiwaaにより 2026/01/14 に作成されました。
//

#include "monokakido/core/platform/fs.hpp"
#include "monokakido/dictionary/catalog.hpp"

#include <iostream>


namespace monokakido::dictionary
{
    DictionaryCatalog& DictionaryCatalog::instance()
    {
        static DictionaryCatalog instance;
        return instance;
    }


    DictionaryCatalog::AccessStatus DictionaryCatalog::checkAccess() const
    {
        if (securityAccess_ && securityAccess_->isValid())
            return AccessStatus::Granted;

        // Attempt to restore access from saved bookmark
        if (const auto bookmark = platform::fs::loadSavedBookmark())
        {
            if (auto access = platform::fs::restoreAccessFromBookmark(*bookmark))
            {
                authorizedPath_ = access->path;
                securityAccess_ = std::move(access->access);
                return AccessStatus::Granted;
            }
        }

        return AccessStatus::NeedsPermission;
    }


    bool DictionaryCatalog::requestAccess() const
    {
        auto bookmarkData = platform::fs::promptForDictionariesAccess();
        if (!bookmarkData)
            return false;

        platform::fs::saveBookmark(bookmarkData->data);

        authorizedPath_ = bookmarkData->resolvedPath;
        securityAccess_ = platform::fs::ScopedSecurityAccess(bookmarkData->resolvedPath);

        if (!securityAccess_->isValid())
        {
            authorizedPath_.reset();
            securityAccess_.reset();
            return false;
        }

        // Clear cache to force refresh
        cachedDictionaries_.reset();
        return true;
    }


    std::expected<std::vector<DictionaryInfo>, std::string> DictionaryCatalog::findAllAvailable() const
    {
        if (const auto status = checkAccess(); status == AccessStatus::NeedsPermission)
            return std::unexpected("Access to dictionaries folder not granted. Please call requestAccess().");

        if (!authorizedPath_)
            return std::unexpected("No authorized path available");

        std::vector<DictionaryInfo> result;
        const auto containerPath = platform::fs::getContainerPathByGroupIdentifier(MONOKAKIDO_GROUP_ID);
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


    std::expected<DictionaryInfo, std::string> DictionaryCatalog::findById(std::string_view id) const
    {
        if (const auto status = checkAccess(); status == AccessStatus::NeedsPermission)
            return std::unexpected("Access to dictionaries folder not granted. Please call requestAccess().");

        if (!authorizedPath_)
            return std::unexpected("No authorized path available");

        if (!cachedDictionaries_)
            findAllAvailable();

        for (const auto& info : cachedDictionaries_.value())
        {
            if (info.id == id)
                return info;
        }

        return std::unexpected(std::format("Failed to find dictionary with id '{}'", id));
    }


    void DictionaryCatalog::refresh() const
    {
        if (cachedDictionaries_)
            cachedDictionaries_.value().clear();

        findAllAvailable();
    }
}

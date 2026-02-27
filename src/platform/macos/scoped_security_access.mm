//
// kiwakiwaaにより 2026/01/15 に作成されました。
//

#include "MKD/platform/macos/scoped_security_access.hpp"

#import <Cocoa/Cocoa.h>
#import <Foundation/Foundation.h>

#include <format>

namespace MKD::macOS
{
    ScopedSecurityAccess::ScopedSecurityAccess(const fs::path& path)
    {
        @autoreleasepool
        {
            NSString* pathStr = [NSString stringWithUTF8String:path.c_str()];
            if (!pathStr) return;

            NSURL* url = [NSURL fileURLWithPath:pathStr];
            if (!url) return;

            if ([url startAccessingSecurityScopedResource])
                url_ = (__bridge void*)[url retain]; // Manually retain since we're not using ARC
        }
    }


    ScopedSecurityAccess::~ScopedSecurityAccess() { release(); }


    ScopedSecurityAccess::ScopedSecurityAccess(ScopedSecurityAccess&& other) noexcept
        : url_(other.url_)
    {
        other.url_ = nullptr;
    }


    ScopedSecurityAccess& ScopedSecurityAccess::operator=(ScopedSecurityAccess&& other) noexcept
    {
        if (this != &other)
        {
            release();
            url_ = other.url_;
            other.url_ = nullptr;
        }
        return *this;
    }


    bool ScopedSecurityAccess::isValid() const { return url_ != nullptr; }


    void ScopedSecurityAccess::release() noexcept
    {
        if (url_)
        {
            @autoreleasepool
            {
                auto url = (__bridge NSURL*)url_;
                [url stopAccessingSecurityScopedResource];
                [url release];
            }
            url_ = nullptr;
        }
    }


    std::expected<BookmarkAccess, std::string> restoreAccessFromBookmark(const std::vector<uint8_t>& bookmarkData)
    {
        @autoreleasepool
        {
            NSData* data = [NSData dataWithBytes:bookmarkData.data()
                                          length:bookmarkData.size()];

            BOOL isStale = NO;
            NSError* error = nil;
            NSURL* url = [NSURL URLByResolvingBookmarkData:data
                options:NSURLBookmarkResolutionWithSecurityScope
                relativeToURL:nil
                bookmarkDataIsStale:&isStale
                error:&error];

            if (!url || error)
                return std::unexpected(std::format("Failed to resolve bookmark: {}", error.localizedDescription.UTF8String));

            if (isStale)
                return std::unexpected("Bookmark is stale, please re-grant access");

            auto path = fs::path{url.path.UTF8String};
            auto access = ScopedSecurityAccess(path);

            if (!access.isValid())
                return std::unexpected("Failed to access security-scoped resource");

            return BookmarkAccess{std::move(path), std::move(access)};
        }
    }


    std::optional<BookmarkData> createSecurityScopedBookmark(const fs::path& path)
    {
        @autoreleasepool
        {
            NSString* pathStr = [NSString stringWithUTF8String:path.c_str()];
            NSURL* url = [NSURL fileURLWithPath:pathStr];
            if (!url) return std::nullopt;

            NSError* error = nil;
            NSData* bookmark = [url bookmarkDataWithOptions:
                                    NSURLBookmarkCreationWithSecurityScope |
                                    NSURLBookmarkCreationSecurityScopeAllowOnlyReadAccess
                                    includingResourceValuesForKeys:nil
                                    relativeToURL:nil
                                    error:&error];

            if (!bookmark || error) return std::nullopt;

            BookmarkData result;
            result.data.assign(static_cast<const uint8_t*>(bookmark.bytes), static_cast<const uint8_t*>(bookmark.bytes) + bookmark.length);
            result.resolvedPath = path;
            return result;
        }
    }
}
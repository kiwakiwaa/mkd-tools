#include "monokakido/platform/macos/scoped_security_access.hpp"

#import <Cocoa/Cocoa.h>
#import <Foundation/Foundation.h>

#include <format>

namespace monokakido::macos
{
    ScopedSecurityAccess::ScopedSecurityAccess(const std::filesystem::path& path)
    {
        @autoreleasepool
        {
            NSString* pathStr = [NSString stringWithUTF8String:path.c_str()];
            if (!pathStr) return;

            NSURL* url = [NSURL fileURLWithPath:pathStr];
            if (!url) return;

            if ([url startAccessingSecurityScopedResource])
            {
                // Manually retain since we're not using ARC
                url_ = (__bridge void*)[url retain];
            }
        }
    }

    ScopedSecurityAccess::~ScopedSecurityAccess()
    {
        release();
    }


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


    bool ScopedSecurityAccess::isValid() const
    {
        return url_ != nullptr;;
    }


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

            auto path = std::filesystem::path{url.path.UTF8String};
            auto access = ScopedSecurityAccess(path);

            if (!access.isValid())
                return std::unexpected("Failed to access security-scoped resource");

            return BookmarkAccess{std::move(path), std::move(access)};
        }
    }


    std::optional<BookmarkData> promptForDictionariesAccess()
    {
        @autoreleasepool
        {
            NSOpenPanel* panel = [NSOpenPanel openPanel];
            panel.message = @"Please select the Monokakido Dictionaries folder to grant access";
            panel.prompt = @"Grant Access";
            panel.canChooseFiles = NO;
            panel.canChooseDirectories = YES;
            panel.allowsMultipleSelection = NO;

            // Optional: try to navigate to expected location
            NSString* groupId = @"group.jp.monokakido.Dictionaries";
            NSURL* containerURL = [[NSFileManager defaultManager]
                containerURLForSecurityApplicationGroupIdentifier:groupId];
            if (containerURL)
            {
                NSURL* dictPath = [containerURL URLByAppendingPathComponent:
                    @"Library/Application Support/com.dictionarystore/dictionaries"];
                panel.directoryURL = dictPath;
            }

            if ([panel runModal] != NSModalResponseOK)
                return std::nullopt;

            NSURL* selectedURL = panel.URLs.firstObject;
            if (!selectedURL)
                return std::nullopt;

            // Create security-scoped bookmark
            NSError* error = nil;
            NSData* bookmark = [selectedURL bookmarkDataWithOptions:
                NSURLBookmarkCreationWithSecurityScope |
                NSURLBookmarkCreationSecurityScopeAllowOnlyReadAccess
                includingResourceValuesForKeys:nil
                relativeToURL:nil
                error:&error];

            if (!bookmark || error)
            {
                NSLog(@"Failed to create bookmark: %@", error);
                return std::nullopt;
            }

            BookmarkData result;
            result.data.assign(
                static_cast<const uint8_t*>(bookmark.bytes),
                static_cast<const uint8_t*>(bookmark.bytes) + bookmark.length
            );
            result.resolvedPath = std::filesystem::path{selectedURL.path.UTF8String};

            return result;
        }
    }


    // Simple preference storage using UserDefaults
    std::optional<std::vector<uint8_t>> loadSavedBookmark()
    {
        @autoreleasepool
        {
            NSData* data = [[NSUserDefaults standardUserDefaults]
                dataForKey:@"MonokakidoDictionariesBookmark"];

            if (!data)
                return std::nullopt;

            std::vector<uint8_t> result;
            result.assign(
                static_cast<const uint8_t*>(data.bytes),
                static_cast<const uint8_t*>(data.bytes) + data.length
            );
            return result;
        }
    }


    void saveBookmark(const std::vector<uint8_t>& bookmarkData)
    {
        @autoreleasepool
        {
            NSData* data = [NSData dataWithBytes:bookmarkData.data()
                                          length:bookmarkData.size()];
            [[NSUserDefaults standardUserDefaults]
                setObject:data forKey:@"MonokakidoDictionariesBookmark"];
        }
    }


    void clearSavedBookmark()
    {
        @autoreleasepool
        {
            [[NSUserDefaults standardUserDefaults] removeObjectForKey:@"MonokakidoDictionariesBookmark"];
        }
    }
}
//
// kiwakiwaaにより 2026/02/27 に作成されました。
//

#include "MKD/platform/macos/bookmark_store.hpp"

#import <Foundation/Foundation.h>

namespace MKD::macOS
{
    static NSString* toNSString(std::string_view sv)
    {
        return [[NSString alloc] initWithBytes:sv.data()
                                 length:sv.size()
                                 encoding:NSUTF8StringEncoding];
    }

    std::optional<std::vector<uint8_t>> loadSavedBookmark(std::string_view key)
    {
        @autoreleasepool
        {
            NSData* data = [[NSUserDefaults standardUserDefaults] dataForKey:toNSString(key)];
            if (!data) return std::nullopt;

            return std::vector<uint8_t>{
                static_cast<const uint8_t*>(data.bytes),
                static_cast<const uint8_t*>(data.bytes) + data.length
            };
        }
    }

    void saveBookmark(const std::vector<uint8_t>& bookmarkData, std::string_view key)
    {
        @autoreleasepool
        {
            NSData* data = [NSData dataWithBytes:bookmarkData.data()
                                          length:bookmarkData.size()];
            [[NSUserDefaults standardUserDefaults] setObject:data forKey:toNSString(key)];
        }
    }

    void clearSavedBookmark(std::string_view key)
    {
        @autoreleasepool
        {
            [[NSUserDefaults standardUserDefaults] removeObjectForKey:toNSString(key)];
        }
    }
}
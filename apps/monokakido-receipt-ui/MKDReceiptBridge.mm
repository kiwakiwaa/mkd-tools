//
// kiwakiwaaにより 2026/03/27 に作成されました。
//

#import "MKDReceiptBridge.h"

#ifdef nil
#define MKD_RESTORE_NIL_MACRO
#pragma push_macro("nil")
#undef nil
#endif

#include "MKD/dictionary/dictionary_product.hpp"
#include "MKD/platform/macos/macos_dictionary_source.hpp"
#include "MKD/platform/macos/monokakido_receipt.hpp"

#ifdef MKD_RESTORE_NIL_MACRO
#pragma pop_macro("nil")
#undef MKD_RESTORE_NIL_MACRO
#endif

#include <filesystem>
#include <ranges>
#include <unordered_map>
#include <vector>

static NSString * const MKDReceiptErrorDomain = @"com.kiwakiwaa.mkd.receipt";

namespace
{
    NSString *ToNSString(const std::string &value)
    {
        return [[NSString alloc] initWithBytes:value.data() length:value.size() encoding:NSUTF8StringEncoding] ?: @"";
    }

    NSString *PathToNSString(const std::filesystem::path &value)
    {
        const auto str = value.string();
        return ToNSString(str);
    }

    NSError *MakeError(const std::string &message)
    {
        return [NSError errorWithDomain:MKDReceiptErrorDomain
                                   code:1
                               userInfo:@{NSLocalizedDescriptionKey: ToNSString(message)}];
    }
}

@implementation MKDReceiptDictionaryItem

- (instancetype)initWithDictionaryTitle:(NSString *)dictionaryTitle
                             productId:(NSString *)productId
                           dictionaryId:(NSString *)dictionaryId
                        purchaseDate:(NSString *)purchaseDate
                            iconPath:(NSString *)iconPath
                          hasReceipt:(BOOL)hasReceipt
{
    self = [super init];
    if (!self) {
        return nil;
    }

    _dictionaryTitle = [dictionaryTitle copy];
    _productId = [productId copy];
    _dictionaryId = [dictionaryId copy];
    _purchaseDate = [purchaseDate copy];
    _iconPath = [iconPath copy];
    _hasReceipt = hasReceipt;
    return self;
}

@end

@implementation MKDReceiptSnapshot

- (instancetype)initWithItems:(NSArray<MKDReceiptDictionaryItem *> *)items
                         missingCount:(NSUInteger)missingCount
{
    self = [super init];
    if (!self) {
        return nil;
    }

    _items = [items copy];
    _missingCount = missingCount;
    return self;
}

@end

@implementation MKDReceiptBridge
{
    MKD::MonokakidoReceiptService _service;
    MKD::MacOSDictionarySource _source;
}

- (BOOL)ensureAccessPrompting:(BOOL)prompt error:(NSError *__autoreleasing  _Nullable *)error
{
    if (_source.checkAccess()) {
        return YES;
    }

    if (prompt && _source.requestAccess(true)) {
        return YES;
    }

    if (error) {
        *error = MakeError("Access to Monokakido dictionaries was not granted");
    }
    return NO;
}

- (MKDReceiptSnapshot *)loadSnapshotWithError:(NSError *__autoreleasing  _Nullable *)error
{
    auto dictionaries = _source.findAllAvailable();
    if (!dictionaries) {
        if (error) {
            *error = MakeError(dictionaries.error());
        }
        return nil;
    }

    struct InstalledDictionaryEntry
    {
        std::string dictionaryId;
        std::string productId;
        std::string title;
        std::optional<std::filesystem::path> iconPath;
    };

    std::vector<InstalledDictionaryEntry> installed;
    installed.reserve(dictionaries->size());
    std::vector<std::string> productIds;
    productIds.reserve(dictionaries->size());

    for (const auto &dictionary : *dictionaries) {
        auto product = MKD::DictionaryProduct::openAtPath(dictionary.path);
        if (!product) {
            if (error) {
                *error = MakeError(product.error());
            }
            return nil;
        }

        InstalledDictionaryEntry entry{
            .dictionaryId = dictionary.id,
            .productId = product->metadata().productIdentifier,
            .title = product->displayTitle(),
            .iconPath = product->iconPath(),
        };
        productIds.push_back(entry.productId);
        installed.push_back(std::move(entry));
    }

    auto inspection = MKD::MonokakidoReceiptService::inspectProducts(std::move(productIds));
    if (!inspection) {
        if (error) {
            *error = MakeError(inspection.error());
        }
        return nil;
    }

    std::unordered_map<std::string, MKD::MonokakidoReceiptStatus> receiptByProduct;
    for (auto &entry : inspection->products) {
        receiptByProduct.emplace(entry.productId, std::move(entry));
    }

    std::ranges::sort(installed, [](const InstalledDictionaryEntry &lhs, const InstalledDictionaryEntry &rhs) {
        return lhs.title < rhs.title;
    });

    NSMutableArray<MKDReceiptDictionaryItem *> *items = [NSMutableArray arrayWithCapacity:installed.size()];

    for (const auto &entry : installed) {
        const auto it = receiptByProduct.find(entry.productId);
        const auto *status = it != receiptByProduct.end() ? &it->second : nullptr;
        NSString *purchaseDate = (status && status->purchaseDate) ? ToNSString(*status->purchaseDate) : nil;
        NSString *iconPath = entry.iconPath ? PathToNSString(*entry.iconPath) : nil;

        MKDReceiptDictionaryItem *item = [[MKDReceiptDictionaryItem alloc]
                                          initWithDictionaryTitle:ToNSString(entry.title)
                                          productId:ToNSString(entry.productId)
                                          dictionaryId:ToNSString(entry.dictionaryId)
                                          purchaseDate:purchaseDate
                                          iconPath:iconPath
                                          hasReceipt:status ? status->hasReceipt : NO];
        [items addObject:item];
    }

    return [[MKDReceiptSnapshot alloc]
            initWithItems:items
            missingCount:inspection->missingReceiptCount];
}

- (NSUInteger)generateMissingPurchasesWithError:(NSError *__autoreleasing  _Nullable *)error
{
    auto dictionaries = _source.findAllAvailable();
    if (!dictionaries) {
        if (error) {
            *error = MakeError(dictionaries.error());
        }
        return 0;
    }

    std::vector<std::string> productIds;
    productIds.reserve(dictionaries->size());
    for (const auto &dictionary : *dictionaries) {
        auto product = MKD::DictionaryProduct::openAtPath(dictionary.path);
        if (!product) {
            if (error) {
                *error = MakeError(product.error());
            }
            return 0;
        }
        productIds.push_back(product->metadata().productIdentifier);
    }

    auto result = MKD::MonokakidoReceiptService::generateMissingPurchases(std::move(productIds));
    if (!result) {
        if (error) {
            *error = MakeError(result.error());
        }
        return 0;
    }

    return result.value();
}

@end

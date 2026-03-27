//
// kiwakiwaaにより 2026/03/27 に作成されました。
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface MKDReceiptDictionaryItem : NSObject
@property (nonatomic, copy, readonly) NSString *dictionaryTitle;
@property (nonatomic, copy, readonly) NSString *productId;
@property (nonatomic, copy, readonly) NSString *dictionaryId;
@property (nonatomic, copy, nullable, readonly) NSString *purchaseDate;
@property (nonatomic, copy, nullable, readonly) NSString *iconPath;
@property (nonatomic, assign, readonly) BOOL hasReceipt;

- (instancetype)initWithDictionaryTitle:(NSString *)dictionaryTitle
                             productId:(NSString *)productId
                           dictionaryId:(NSString *)dictionaryId
                        purchaseDate:(nullable NSString *)purchaseDate
                            iconPath:(nullable NSString *)iconPath
                          hasReceipt:(BOOL)hasReceipt;
@end

@interface MKDReceiptSnapshot : NSObject
@property (nonatomic, copy, readonly) NSArray<MKDReceiptDictionaryItem *> *items;
@property (nonatomic, assign, readonly) NSUInteger missingCount;

- (instancetype)initWithItems:(NSArray<MKDReceiptDictionaryItem *> *)items
                         missingCount:(NSUInteger)missingCount;
@end

@interface MKDReceiptBridge : NSObject
- (BOOL)ensureAccessPrompting:(BOOL)prompt error:(NSError * _Nullable * _Nullable)error;
- (nullable MKDReceiptSnapshot *)loadSnapshotWithError:(NSError * _Nullable * _Nullable)error;
- (NSUInteger)generateMissingPurchasesWithError:(NSError * _Nullable * _Nullable)error;
@end

NS_ASSUME_NONNULL_END

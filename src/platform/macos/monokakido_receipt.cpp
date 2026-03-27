//
// kiwakiwaaにより 2026/03/27 に作成されました。
//

#include "MKD/platform/macos/monokakido_receipt.hpp"
#include "MKD/platform/macos/macos_dictionary_source.hpp"

#include "fs.hpp"

#include <CryptoKit/CryptoKit.hpp>
#include <Foundation/Foundation.hpp>

#ifdef nil
#undef nil
#endif

#include <glaze/glaze.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <format>
#include <fstream>
#include <span>
#include <unordered_map>

namespace MKD::detail
{
    struct ReceiptPurchase
    {
        std::string original_transaction_id;
        std::string product_id;
        std::string purchase_date;
        std::string transaction_id;
    };

    struct ReceiptPayload
    {
        bool isSandbox = false;
        std::unordered_map<std::string, std::unordered_map<std::string, std::string>> migrations;
        std::unordered_map<std::string, ReceiptPurchase> purchases;
        std::string receiptIdentifier;
        std::vector<std::string> revokedTransactions;
        std::vector<std::string> unitedPurchases;
        std::vector<std::string> uploadedTransactions;
        std::string vendorIdentifier;
        std::string version;
    };
}

template<>
struct glz::meta<MKD::detail::ReceiptPurchase>
{
    using T = MKD::detail::ReceiptPurchase;
    static constexpr auto value = object(
        "original_transaction_id", &T::original_transaction_id,
        "product_id", &T::product_id,
        "purchase_date", &T::purchase_date,
        "transaction_id", &T::transaction_id
    );
};

template<>
struct glz::meta<MKD::detail::ReceiptPayload>
{
    using T = MKD::detail::ReceiptPayload;
    static constexpr auto value = object(
        "isSandbox", &T::isSandbox,
        "migrations", &T::migrations,
        "purchases", &T::purchases,
        "receiptIdentifier", &T::receiptIdentifier,
        "revokedTransactions", &T::revokedTransactions,
        "unitedPurchases", &T::unitedPurchases,
        "uploadedTransactions", &T::uploadedTransactions,
        "vendorIdentifier", &T::vendorIdentifier,
        "version", &T::version
    );
};

namespace MKD
{
    namespace
    {
        constexpr auto RECEIPT_SUBPATH = "Library/Application Support/com.dictionarystore/receipt/purchases";

        struct KeyEntry
        {
            uint8_t type;
            uint64_t sortIndex;
            std::string_view value;
        };

        // putting the key in a table like this does not make it more difficult to recover
        constexpr std::array<KeyEntry, 28> kStaticKeyTable = {
            {
                {0, 1, "bbShLjmhaV"},
                {2, 1, "OlV8fmxZ"},
                {4, 1, "7z7VyahPQH"},
                {5, 2, "98Twc14KMC"},
                {1, 3, "wanWsHSA"},
                {4, 0, "RJhMQAHSMI"},
                {0, 0, "3uHgW6dtuU"},
                {6, 2, "I4r6goGp1D"},
                {5, 1, "UhuNQBMfPB"},
                {1, 2, "jSvouCLx"},
                {3, 1, "7EaYDyRbBm"},
                {4, 3, "Jj5IMV3z3a"},
                {6, 3, "BF2Zmuf4v3"},
                {2, 0, "PEkePQtM"},
                {2, 2, "bBw2fyCy"},
                {5, 0, "ovgo2RECNy"},
                {6, 1, "2z1lRC1N0d"},
                {4, 2, "Q8uQUiVNEA"},
                {0, 3, "ypxxEhGHg0"},
                {3, 2, "g9m13rIzaM"},
                {5, 3, "81DBYONVIg"},
                {1, 0, "rHDMgl1P"},
                {3, 0, "SvzB9MMbS0"},
                {0, 2, "iTGIReQjhJ"},
                {1, 1, "SWorXjqg"},
                {2, 3, "Pe8bYFtu"},
                {3, 3, "FD9VD0u58g"},
                {6, 0, "Q1Bn7yH5pR"},
            }
        };

        template<typename T>
        bool contains(const std::vector<T>& values, const T& value)
        {
            return std::ranges::find(values, value) != values.end();
        }

        std::string deriveKeyMaterial(const uint8_t type)
        {
            std::array<KeyEntry, kStaticKeyTable.size()> sorted = kStaticKeyTable;
            std::ranges::sort(sorted, [](const KeyEntry& lhs, const KeyEntry& rhs) {
                if (lhs.type != rhs.type) return lhs.type < rhs.type;
                return lhs.sortIndex < rhs.sortIndex;
            });

            std::string result;
            for (const auto& entry : sorted)
            {
                if (entry.type == type)
                    result += entry.value;
            }
            return result;
        }

        Result<std::vector<uint8_t>> readBytes(const fs::path& path)
        {
            std::ifstream file(path, std::ios::binary | std::ios::ate);
            if (!file)
                return std::unexpected(std::format("Failed to open file: {}", path.string()));

            const auto end = file.tellg();
            if (end < 0)
                return std::unexpected(std::format("Failed to determine file size: {}", path.string()));

            std::vector<uint8_t> bytes(end);
            file.seekg(0);
            if (!bytes.empty() && !file.read(reinterpret_cast<char*>(bytes.data()),
                                             static_cast<std::streamsize>(bytes.size())))
                return std::unexpected(std::format("Failed to read file: {}", path.string()));

            return bytes;
        }

        Result<void> writeBytes(const fs::path& path, const std::span<const uint8_t> bytes)
        {
            std::ofstream file(path, std::ios::binary | std::ios::trunc);
            if (!file)
                return std::unexpected(std::format("Failed to open file for writing: {}", path.string()));

            if (!bytes.empty())
                file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));

            if (!file.good())
                return std::unexpected(std::format("Failed to write file: {}", path.string()));

            return {};
        }

        Result<std::vector<uint8_t>> decryptReceipt(const std::span<const uint8_t> encrypted)
        {
            if (encrypted.size() < 28)
                return std::unexpected("Encrypted receipt is too small");

            NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();

            const std::string keyMaterial = deriveKeyMaterial(1);
            if (keyMaterial.size() != 32)
            {
                pool->drain();
                return std::unexpected("Derived key material is not 32 bytes");
            }

            auto* keyData = NS::Data::dataWithBytes(keyMaterial.data(), keyMaterial.size());
            auto* key = CK::SymmetricKey::symmetricKey(keyData);
            if (!key)
            {
                pool->drain();
                return std::unexpected("Failed to construct receipt symmetric key");
            }

            auto* combined = NS::Data::dataWithBytes(encrypted.data(), encrypted.size());
            auto* box = CK::SealedBox::sealedBox(combined);
            if (!box)
            {
                pool->drain();
                return std::unexpected("Failed to parse receipt as ChaChaPoly sealed box");
            }

            const auto* decrypted = CK::ChaChaPoly::open(box, key);
            if (!decrypted)
            {
                pool->drain();
                return std::unexpected("Failed to decrypt receipt");
            }

            std::vector<uint8_t> result(decrypted->length());
            if (!result.empty())
                std::memcpy(result.data(), decrypted->bytes(), result.size());

            pool->drain();
            return result;
        }

        Result<std::vector<uint8_t>> encryptReceipt(const std::span<const uint8_t> plaintext)
        {
            NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();

            const std::string keyMaterial = deriveKeyMaterial(1);
            if (keyMaterial.size() != 32)
            {
                pool->drain();
                return std::unexpected("Derived key material is not 32 bytes");
            }

            auto* keyData = NS::Data::dataWithBytes(keyMaterial.data(), keyMaterial.size());
            auto* key = CK::SymmetricKey::symmetricKey(keyData);
            if (!key)
            {
                pool->drain();
                return std::unexpected("Failed to construct receipt symmetric key");
            }

            auto* plainData = NS::Data::dataWithBytes(plaintext.data(), plaintext.size());
            const auto* box = CK::ChaChaPoly::seal(plainData, key);
            if (!box)
            {
                pool->drain();
                return std::unexpected("Failed to encrypt receipt");
            }

            const auto* combined = box->combined();
            if (!combined)
            {
                pool->drain();
                return std::unexpected("Failed to read encrypted combined bytes");
            }

            std::vector<uint8_t> encrypted(combined->length());
            if (!encrypted.empty())
                std::memcpy(encrypted.data(), combined->bytes(), encrypted.size());

            pool->drain();
            return encrypted;
        }

        Result<detail::ReceiptPayload> loadReceiptPayload(const fs::path& purchasesPath)
        {
            auto encrypted = readBytes(purchasesPath);
            if (!encrypted)
                return std::unexpected(encrypted.error());

            auto decrypted = decryptReceipt(*encrypted);
            if (!decrypted)
                return std::unexpected(decrypted.error());

            std::string jsonText(reinterpret_cast<const char*>(decrypted->data()), decrypted->size());
            detail::ReceiptPayload payload{};
            if (const auto err = glz::read<glz::opts{.error_on_unknown_keys = false}>(payload, jsonText))
            {
                return std::unexpected(
                    "Failed to parse decrypted receipt JSON: " + std::string(glz::format_error(err, jsonText)));
            }

            return payload;
        }

        Result<void> saveReceiptPayload(const fs::path& purchasesPath, const detail::ReceiptPayload& payload)
        {
            std::string json;
            if (const auto err = glz::write_json(payload, json))
                return std::unexpected("Failed to serialize receipt JSON: " + glz::format_error(err, json));

            auto encrypted = encryptReceipt(std::span{reinterpret_cast<const uint8_t*>(json.data()), json.size()});
            if (!encrypted)
                return std::unexpected(encrypted.error());

            return writeBytes(purchasesPath, *encrypted);
        }

        Result<fs::path> resolvePurchasesPath()
        {
            auto purchases = macOS::getMonokakidoDictionariesPath(MONOKAKIDO_GROUP_ID, RECEIPT_SUBPATH);
            if (!purchases)
                return std::unexpected("Unable to resolve Monokakido purchases path");
            return *purchases;
        }

        const detail::ReceiptPurchase* findPurchaseByProductId(const detail::ReceiptPayload& payload,
                                                               std::string_view productId)
        {
            if (const auto it = payload.purchases.find(std::string(productId)); it != payload.purchases.end())
                return &it->second;

            const auto iter = std::ranges::find_if(payload.purchases, [productId](const auto& entry) {
                return entry.second.product_id == productId;
            });
            return iter != payload.purchases.end() ? &iter->second : nullptr;
        }

        std::string makeTransactionId(const size_t sequence)
        {
            const auto now = std::chrono::system_clock::now();
            const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
            return std::format("{}{:03}", millis, sequence % 1000);
        }

        std::string makePurchaseDate()
        {
            const auto now = std::chrono::system_clock::now();
            const auto utc = std::chrono::floor<std::chrono::seconds>(now);
            return std::format("{:%Y-%m-%dT%H:%M:%SZ}", utc);
        }
    }

    Result<MonokakidoReceiptInspection> MonokakidoReceiptService::inspectProducts(
        std::vector<std::string> productIds)
    {
        std::ranges::sort(productIds);
        productIds.erase(std::ranges::unique(productIds).begin(), productIds.end());

        auto purchasesPath = resolvePurchasesPath();
        if (!purchasesPath)
            return std::unexpected(purchasesPath.error());

        auto payload = loadReceiptPayload(*purchasesPath);
        if (!payload)
            return std::unexpected(payload.error());

        MonokakidoReceiptInspection inspection{};
        inspection.purchasesPath = *purchasesPath;

        for (const auto& productId : productIds)
        {
            MonokakidoReceiptStatus status{};
            status.productId = productId;

            if (const auto* purchase = findPurchaseByProductId(*payload, productId))
            {
                status.hasReceipt = true;
                if (!purchase->purchase_date.empty())
                    status.purchaseDate = purchase->purchase_date;
            }
            else
            {
                inspection.missingReceiptCount++;
            }

            inspection.products.push_back(std::move(status));
        }

        return inspection;
    }

    Result<size_t> MonokakidoReceiptService::generateMissingPurchases(std::vector<std::string> productIds)
    {
        std::ranges::sort(productIds);
        productIds.erase(std::ranges::unique(productIds).begin(), productIds.end());

        auto purchasesPath = resolvePurchasesPath();
        if (!purchasesPath)
            return std::unexpected(purchasesPath.error());

        auto payload = loadReceiptPayload(*purchasesPath);
        if (!payload)
            return std::unexpected(payload.error());

        size_t generatedCount = 0;

        for (const auto& productId : productIds)
        {
            if (findPurchaseByProductId(*payload, productId))
                continue;

            const std::string purchaseDate = makePurchaseDate();
            const std::string transactionId = makeTransactionId(generatedCount + 1);

            payload->purchases[productId] = detail::ReceiptPurchase{
                .original_transaction_id = transactionId,
                .product_id = productId,
                .purchase_date = purchaseDate,
                .transaction_id = transactionId,
            };

            if (!contains(payload->unitedPurchases, productId))
                payload->unitedPurchases.push_back(productId);

            if (!contains(payload->uploadedTransactions, transactionId))
                payload->uploadedTransactions.push_back(transactionId);

            generatedCount++;
        }

        if (generatedCount == 0)
            return size_t{0};

        if (auto save = saveReceiptPayload(*purchasesPath, *payload); !save)
            return std::unexpected(save.error());

        return generatedCount;
    }
}

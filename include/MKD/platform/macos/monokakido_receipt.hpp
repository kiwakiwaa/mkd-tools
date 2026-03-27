//
// kiwakiwaaにより 2026/03/27 に作成されました。
//

#pragma once

#include "MKD/result.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace MKD
{
    namespace fs = std::filesystem;

    struct MonokakidoReceiptStatus
    {
        std::string productId;
        std::optional<std::string> purchaseDate;
        bool hasReceipt = false;
    };

    struct MonokakidoReceiptInspection
    {
        fs::path purchasesPath;
        std::vector<MonokakidoReceiptStatus> products;
        size_t missingReceiptCount = 0;
    };

    class MonokakidoReceiptService
    {
    public:
        static Result<MonokakidoReceiptInspection> inspectProducts(std::vector<std::string> productIds);

        // Generates and writes purchase records for products that are installed but missing receipts.
        // Returns the number of added purchase records.
        static Result<size_t> generateMissingPurchases(std::vector<std::string> productIds) ;
    };
}

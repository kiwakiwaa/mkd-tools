//
// kiwakiwaaにより 2026/02/24 に作成されました。
//

#pragma once

#include "MKD/dictionary/metadata.hpp"
#include "MKD/dictionary/paths.hpp"
#include "MKD/dictionary/dictionary.hpp"

namespace MKD
{
    class DictionaryProduct
    {
    public:
        static std::expected<DictionaryProduct, std::string> openAtPath(const fs::path& path);

        [[nodiscard]] const DictionaryMetadata& metadata() const noexcept { return metadata_; }
        [[nodiscard]] const std::vector<Dictionary>& dictionaries() const noexcept { return dictionaries_; }
        [[nodiscard]] std::vector<Dictionary>& dictionaries() noexcept { return dictionaries_; }

        // Find a specific content dictionary by its content identifier
        [[nodiscard]] const Dictionary* findContent(std::string_view contentId) const;
        [[nodiscard]] Dictionary* findContent(std::string_view contentId);

    private:
        DictionaryProduct(DictionaryMetadata metadata, DictionaryPaths paths,
                          std::vector<Dictionary> dictionaries);

        DictionaryMetadata metadata_;
        DictionaryPaths paths_;
        std::vector<Dictionary> dictionaries_;
    };
}
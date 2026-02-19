//
// kiwakiwaaにより 2026/02/19 に作成されました。
//

#pragma once

#include "dictionary_source.hpp"

#include <string_view>

namespace monokakido {
    class DirectoryDictionarySource : public DictionarySource {
    public:
        explicit DirectoryDictionarySource(fs::path root);

        [[nodiscard]] std::expected<std::vector<DictionaryInfo>, std::string> findAllAvailable() const override;

        [[nodiscard]] std::expected<DictionaryInfo, std::string> findById(std::string_view dictId) const override;

    private:

        static bool looksLikeDictionary(const fs::path& path);

        std::filesystem::path root_;
    };
}
//
// kiwakiwaaにより 2026/03/11 に作成されました。
//

#pragma once

#include "MKD/result.hpp"
#include "MKD/resource/entry_id.hpp"
#include "resource/keystore/keystore_search.hpp"

#include <memory>
#include <string_view>

namespace MKD
{
    class Dictionary;


    enum class SearchScope : uint8_t
    {
        Headword = 0,
        Idiom = 1,
        Example = 2,
        English = 3,
        Sense = 4,
        Kanji = 6,
        Collocation = 7,
        Fulltext = 10,
        Category = 11,
        Compound = 100,
        Numeral = 101
    };


    struct SearchOptions
    {
        SearchMode type = SearchMode::Prefix;
        size_t limit = 0; // 0 = no limit

        bool enableScopeFallback = true; // try broader scopes on empty result
        bool enableStopWords = true; // remove stop words and retry
        bool enableLemmaLookup = true; // union lemma results at scope 1
        bool enableRomajiFallback = true; // Convert to romaji and retry
        bool enableJapaneseCompound = true; // jp compound decomposition
        bool enableCollocation = false; // when true, also searches collocation keystore on empty result
    };

    struct SearchResult
    {
        std::vector<EntryId> entries;

        //   0 = direct match
        //   4 = scope fallback
        //   8 = jp fallback
        uint32_t flags = 0;

        // keys that actually matched
        std::vector<std::string> matchedKeys;

        [[nodiscard]] size_t size() const noexcept { return entries.size(); }
        [[nodiscard]] bool empty() const noexcept { return entries.empty(); }

        [[nodiscard]] auto begin() const noexcept { return entries.begin(); }
        [[nodiscard]] auto end() const noexcept { return entries.end(); }

        void unionWith(const SearchResult& other);

        void intersectWith(const SearchResult& other);
    };


    class DictionarySearch
    {
    public:
        explicit DictionarySearch(const Dictionary& dict);
        ~DictionarySearch();

        DictionarySearch(DictionarySearch&&) noexcept;
        DictionarySearch& operator=(DictionarySearch&&) noexcept;

        DictionarySearch(const DictionarySearch&) = delete;
        DictionarySearch& operator=(const DictionarySearch&) = delete;

        [[nodiscard]] Result<SearchResult> search(std::string_view query, const SearchOptions& options = {});

        // cancel in progress search
        void cancel() noexcept;

        // reset cancellation
        void reset() noexcept;

        [[nodiscard]] bool isCanccelled() const noexcept;

    private:
        // cascading fallback across scopes
        [[nodiscard]] Result<SearchResult> searchWithKeys(const std::vector<std::string>& keys, size_t limit);

        // iterate keys, search each, intersect
        [[nodiscard]] Result<SearchResult> searchWithKeysAndFlags(const std::vector<std::string>& keys, size_t limit, uint32_t flags);

        // dispatch by SearchType
        [[nodiscard]] Result<SearchResult> searchSingleKey(std::string_view key, size_t limit);

        [[nodiscard]] Result<SearchResult> simpleSearch(std::string_view key, SearchMode type);

        // compound word decomposition with front/back stripping
        [[nodiscard]] Result<SearchResult> japaneseCompoundSearch(std::string_view key);

        struct Impl;
        std::unique_ptr<Impl> impl;
    };
}

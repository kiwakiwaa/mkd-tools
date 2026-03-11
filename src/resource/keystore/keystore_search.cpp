//
// kiwakiwaaにより 2026/03/10 に作成されました。
//

#include "keystore_search.hpp"
#include "keystore_compare.hpp"

#include <algorithm>
#include "utf8.h"


namespace MKD
{
    namespace detail
    {
        keystore::CompareMode compareModeForSearch(const SearchMode mode)
        {
            switch (mode)
            {
                case SearchMode::Prefix:
                case SearchMode::Exact:
                    return keystore::CompareMode::Forward;
                case SearchMode::Suffix:
                    return keystore::CompareMode::Backward;
            }
            std::unreachable();
        }


        KeystoreIndex indexForSearch(const SearchMode mode)
        {
            switch (mode)
            {
                case SearchMode::Prefix:
                case SearchMode::Exact: // todo should use KeystoreIndex::Length later
                    return KeystoreIndex::Prefix;
                case SearchMode::Suffix:
                    return KeystoreIndex::Suffix;
            }
            std::unreachable();
        }


        size_t indexLowerBound(const Keystore& keystore,
                               const KeystoreIndex indexType,
                               std::string_view searchKey,
                               const keystore::CompareMode mode)
        {
            const size_t count = keystore.indexSize(indexType);
            if (count == 0 || searchKey.empty())
                return 0;

            size_t lowerBound = 0;
            size_t rangeSize = count;

            while (rangeSize > 0)
            {
                const size_t mid = lowerBound + (rangeSize / 2);

                // get the entry key at this pos
                // todo: maybe not return all decoded pages here since we only need the key for comparison
                const auto entry = keystore.getByIndex(indexType, mid);
                if (!entry)
                    break;

                if (const int cmp = detail::keystore::compare(entry->key, searchKey, mode); cmp >= 0)
                {
                    rangeSize = rangeSize / 2;
                }
                else
                {
                    lowerBound = mid + 1;
                    rangeSize = rangeSize - (rangeSize / 2) - 1;
                }
            }

            return lowerBound;
        }


        /*
         * Create the "next key" utf8 string for upperbound search
         * returns empty if no upper bound needed
         * that is search to end of index
         */
        std::string createNextKey(std::string_view searchKey, const SearchMode mode)
        {
            const bool reverse = (mode == SearchMode::Suffix);

            std::u32string norm;
            auto it = searchKey.begin();
            const auto end = searchKey.end();

            while (it != end)
            {
                const char32_t cp = utf8::next(it, end);
                if (keystore::isIgnorable(cp)) continue;
                norm.push_back(keystore::foldCase(cp));
            }

            if (reverse)
                std::ranges::reverse(norm);

            if (norm.empty()) return {};

            // increment last cp
            // if max then pop or return empty if all max
            while (!norm.empty() && norm.back() == 0x10FFFF)
                norm.pop_back();

            if (norm.empty()) return {};

            ++norm.back();

            if (reverse)
                std::ranges::reverse(norm);

            std::string result;
            utf8::utf32to8(norm.begin(), norm.end(), std::back_inserter(result));
            return result;
        }
    }


    Result<KeystoreSearchRange> keystoreSearch(const Keystore& keystore, std::string_view query, const SearchMode mode)
    {
        if (query.empty())
            return std::unexpected("Empty search query");

        const auto indexType = detail::indexForSearch(mode);
        const auto cmpMode = detail::compareModeForSearch(mode);
        const size_t indexSize = keystore.indexSize(indexType);

        if (indexSize == 0)
            return std::unexpected("Index is empty or does not exist");

        const size_t lowerBound = detail::indexLowerBound(keystore, indexType, query, cmpMode);
        size_t upperBound = indexSize; // find upper bound via "next key"

        if (lowerBound < indexSize)
        {
            const std::string nextKey = detail::createNextKey(query, mode);
            if (!nextKey.empty())
                upperBound = detail::indexLowerBound(keystore, indexType, nextKey, cmpMode);
        }

        return KeystoreSearchRange{
            .begin = lowerBound,
            .end = upperBound,
            .indexType = indexType
        };
    }


    Result<std::vector<KeystoreLookupResult>> keystoreSearchResults(const Keystore& keystore,
                                                                    std::string_view query,
                                                                    const SearchMode mode)
    {
        auto range = keystoreSearch(keystore, query, mode);
        if (!range) return std::unexpected(range.error());

        std::vector<KeystoreLookupResult> results;
        results.reserve(range->count());

        std::u32string queryNorm;
        if (mode == SearchMode::Exact)
            queryNorm = detail::keystore::normalizeKeyToUTF32(query, false);

        for (size_t i = range->begin; i < range->end; i++)
        {
            auto entry = keystore.getByIndex(range->indexType, i);
            if (!entry) continue; // shouldnt happen

            if (mode == SearchMode::Exact)
            {
                auto entryNorm = detail::keystore::normalizeKeyToUTF32(entry->key, false);
                if (entryNorm != queryNorm)
                    continue;
            }

            results.push_back(std::move(*entry));
        }

        return results;
    }
}
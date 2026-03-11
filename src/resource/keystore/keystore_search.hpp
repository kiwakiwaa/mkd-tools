//
// kiwakiwaaにより 2026/03/10 に作成されました。
//

#pragma once

#include "MKD/result.hpp"
#include "MKD/resource/keystore.hpp"

#include <string_view>

namespace MKD
{
    /*
     * Prefix -> Index B: sorted by normalised key, forward comparison
     * Exact -> Index B: (prefix search + filter for exact match)
     *          TODO: Itd be faster to use Index A. Would require binary search for
     *          length boundary first, then search within same length entries.
     *
     * Suffix -> Index C: sorted by reversed normalised key, backward comparison
     */
    enum class SearchMode
    {
        Prefix,
        Exact,
        Suffix
    };


    struct KeystoreSearchRange
    {
        size_t begin = 0;
        size_t end = 0;
        KeystoreIndex indexType = KeystoreIndex::Prefix;

        [[nodiscard]] size_t count() const noexcept { return end > begin ? end - begin : 0; }

        [[nodiscard]] bool empty() const noexcept { return begin >= end; }
    };


    /**
     * Seach a keystore index, returning the range of matching positions. Reversed from HMDicKeyIndexSearch
     * - Performs two lower bound searches
     *    + one for the search key
     *    + one for the "next key"
     *
     *
     *
     * @param keystore Keystore to search
     * @param query UTF-8 search term
     * @param mode prefix, exact or suffix
     * @return range of matching index positions, or error string
     */
    [[nodiscard]] Result<KeystoreSearchRange> keystoreSearch(const Keystore& keystore, std::string_view query, SearchMode mode);


    /*
     * Collect all results from a search range
     *
     * for exact mode, only entries whose normalised key exactly matches the normalised query are included
     * TODO: use length  index instead
     */
    [[nodiscard]] Result<std::vector<KeystoreLookupResult>> keystoreSearchResults(const Keystore& keystore, std::string_view query, SearchMode mode);

}

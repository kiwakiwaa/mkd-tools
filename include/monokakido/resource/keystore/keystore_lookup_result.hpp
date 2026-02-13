//
// kiwakiwaaにより 2026/02/07 に作成されました。
//

#pragma once

#include "page_reference.hpp"

#include <string_view>
#include <vector>

namespace monokakido
{
    // Result of a keystore lookup, owns the decoded page references.
    struct KeystoreLookupResult
    {
        std::string_view key;                   // The search term (points into Keystore's file buffer)
        size_t index = 0;                       // Position within the queried index
        std::vector<PageReference> pages;       // Decoded page references

        [[nodiscard]] size_t size()  const noexcept { return pages.size(); }
        [[nodiscard]] bool   empty() const noexcept { return pages.empty(); }

        // Range support
        [[nodiscard]] auto begin() const noexcept { return pages.begin(); }
        [[nodiscard]] auto end()   const noexcept { return pages.end(); }
    };
}

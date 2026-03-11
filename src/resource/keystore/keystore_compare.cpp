//
// kiwakiwaaにより 2026/03/11 に作成されました。
//

#include "keystore_compare.hpp"
#include "resource/detail/unicode/unicode_case_map.hpp"

#include "utf8.h"

#include <algorithm>

namespace MKD::detail::keystore
{
    char32_t foldCase(const char32_t cp)
    {
        // ascii uppercase to lowercase
        if (cp >= 0x41 && cp <= 0x5A)
            return cp | 0x20;

        // skip CJK / kana
        if (cp >= 0x3000 && cp < 0xA000)
            return cp;

        // everything else goes through the case map
        return unicode::toLowercase(cp);
    }


    std::u32string normalizeKeyToUTF32(std::string_view s, const bool reverse)
    {
        std::u32string result;
        auto it = s.begin();
        const auto end = s.end();
        while (it != end)
        {
            const char32_t cp = utf8::next(it, end);
            if (isIgnorable(cp)) continue;
            result.push_back(foldCase(cp));
        }

        if (reverse)
            std::ranges::reverse(result);

        return result;
    }


    size_t normalizedLength(std::string_view s)
    {
        auto it = s.begin();
        const auto end = s.end();
        size_t len = 0;
        while (it != end)
        {
            if (const char32_t cp = utf8::next(it, end); !isIgnorable(cp))
                ++len;
        }
        return len;
    }


    int compare(std::string_view a, std::string_view b, CompareMode mode)
    {
        if (mode == CompareMode::Length) // first by codepoint len then forward (prefix) with fold case
        {
            const size_t lenA = normalizedLength(a);
            const size_t lenB = normalizedLength(b);
            if (lenA < lenB) return -1;
            if (lenA > lenB) return 1;

            mode = CompareMode::Forward;
        }

        if (mode == CompareMode::Backward) // for suffix search
        {
            auto pa = a.end(); // reverse both strings
            auto pb = b.end();
            const auto beginA = a.begin();
            const auto beginB = b.begin();

            while (true)
            {
                char32_t ca = 0;
                char32_t cb = 0;

                while (pa != beginA)
                {
                    if (const char32_t cp = utf8::prior(pa, beginA); !isIgnorable(cp))
                    {
                        ca = foldCase(cp);
                        break;
                    }
                }

                while (pb != beginB)
                {
                    if (const char32_t cp = utf8::prior(pb, beginB); !isIgnorable(cp))
                    {
                        cb = foldCase(cp);
                        break;
                    }
                }

                if (ca == 0 && cb == 0) return 0;
                if (ca < cb) return -1;
                if (ca > cb) return 1;
            }
        }
        else // Forward
        {
            auto pa = a.begin();
            auto pb = b.begin();
            const auto endA = a.end();
            const auto endB = b.end();

            while (true)
            {
                char32_t ca = 0;
                char32_t cb = 0;

                while (pa != endA)
                {
                    if (const char32_t cp = utf8::next(pa, endA); !isIgnorable(cp))
                    {
                        ca = foldCase(cp);
                        break;
                    }
                }

                while (pb != endB)
                {
                    if (const char32_t cp = utf8::next(pb, endB); !isIgnorable(cp))
                    {
                        cb = foldCase(cp);
                        break;
                    }
                }

                if (ca == 0 && cb == 0) return 0;
                if (ca < cb) return -1;
                if (ca > cb) return 1;
            }
        }
    }
}
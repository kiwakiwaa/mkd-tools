//
// kiwakiwaaにより 2026/03/07 に作成されました。
//
#include "keystore_writer.hpp"
#include "../detail/binary_buffer.hpp"
#include "../detail/unicode/unicode_utils.hpp"
#include "../detail/unicode/unicode_case_map.hpp"

#include <algorithm>
#include <format>
#include <fstream>
#include <numeric>

namespace MKD
{
    namespace detail
    {
        /**
         * Merge page references into an existing key's list, deduplicating.
         */
        void mergePages(std::vector<PageReference>& existing, const std::span<const PageReference> incoming)
        {
            for (const auto& ref : incoming)
            {
                if (std::ranges::find(existing, ref) == existing.end())
                    existing.push_back(ref);
            }
        }

        /**
         * Normalises a key for case-insensitive comparison.
         *
         * The resulting normalized string can be used for:
         *   - Building sorted indices
         *   - Binary search during lookup
         *   - Comparing keys regardless of case and ignorable characters
         *
         * @param utf8Key Original UTF-8 encoded key
         * @return Normalized UTF-32 string for comparison
         */
        std::u32string normalizeForPrefixCompare(const std::string& utf8Key)
        {
            std::u32string result;
            result.reserve(utf8Key.size());

            for (const std::u32string codepoints = detail::unicode::toUtf32(utf8Key); const char32_t cp : codepoints)
            {
                // Skip spaces and hyphens
                if (cp == 0x20 || cp == 0x2D)
                    continue;

                if (cp >= 0x41 && cp <= 0x5A) // ASCII
                    result.push_back(cp | 0x20);
                else if (cp >= 0x3000 && cp < 0xA000)
                    result.push_back(cp); // skip CJK/kana range
                else
                    result.push_back(detail::unicode::toLowercase(cp)); // everything else goes through the case map
            }

            return result;
        }
    }


    KeystoreWriter::KeystoreWriter(const uint32_t version)
        : version_(version)
    {
    }


    Result<void> KeystoreWriter::add(std::string_view key, std::vector<PageReference> pages)
    {
        if (key.empty())
            return std::unexpected("Search key must not be empty");

        if (pages.empty())
            return std::unexpected("Page references must not be empty");

        auto& existing = entries_[std::string(key)];
        detail::mergePages(existing, pages);
        return {};
    }


    Result<void> KeystoreWriter::addPage(PageReference page, const std::span<const std::string_view> keys)
    {
        if (keys.empty())
            return std::unexpected("Keys span must not be empty");

        for (auto key : keys)
        {
            if (key.empty()) continue;
            auto& existing = entries_[std::string(key)];
            detail::mergePages(existing, std::span(&page, 1));
        }
        return {};
    }

    Result<void> KeystoreWriter::finalize(const fs::path& filePath) const
    {
        auto entries = collectEntries();

        auto words = buildWordsSection(entries);
        if (!words)
            return std::unexpected(words.error());

        auto idx = buildIndexArrays(entries, *words);

        // serialise each index array
        auto indexA = serializeIndexArray(idx.length);
        auto indexB = serializeIndexArray(idx.prefix);
        auto indexC = serializeIndexArray(idx.suffix);
        auto indexD = serializeIndexArray(idx.other);

        // calculate file layout offsets
        const bool isV2 = version_ == KEYSTORE_V2;
        const size_t headerSize = isV2 ? 32 : 16;
        const size_t wordsOffset = headerSize;
        const size_t indexSectionOffset = wordsOffset + words->data.size();
        constexpr size_t indexHeaderSize = 20; // IndexHeader: magic + 4 offsets

        constexpr size_t indexAOffset = indexHeaderSize;
        const size_t indexBOffset = indexAOffset + indexA.size();
        const size_t indexCOffset = indexBOffset + indexB.size();
        const size_t indexDOffset = indexCOffset + indexC.size();

        std::ofstream file(filePath, std::ios::binary | std::ios::trunc);
        if (!file)
            return std::unexpected(std::format("Failed to open: {}", filePath.string()));

        // File header
        {
            detail::BinaryBuffer header(headerSize);
            header.writeLE(version_);
            header.writeLE(uint32_t{0}); // magic1
            header.writeLE(static_cast<uint32_t>(wordsOffset));
            header.writeLE(static_cast<uint32_t>(indexSectionOffset));

            if (isV2)
            {
                header.writeLE(uint32_t{0}); // conversionTable not used
                header.writeLE(uint32_t{0}); // magic5
                header.writeLE(uint32_t{0}); // magic6
                header.writeLE(uint32_t{0}); // magic7
            }

            file.write(reinterpret_cast<const char*>(header.data().data()),
                       static_cast<std::streamsize>(header.data().size()));
        }

        file.write(reinterpret_cast<const char*>(words->data.data()), static_cast<std::streamsize>(words->data.size()));

        {
            detail::BinaryBuffer indexHeader(indexHeaderSize);
            indexHeader.writeLE(INDEX_MAGIC);
            indexHeader.writeLE(static_cast<uint32_t>(indexAOffset));
            indexHeader.writeLE(static_cast<uint32_t>(indexBOffset));
            indexHeader.writeLE(static_cast<uint32_t>(indexCOffset));
            indexHeader.writeLE(static_cast<uint32_t>(indexDOffset));

            file.write(reinterpret_cast<const char*>(indexHeader.data().data()),
                       static_cast<std::streamsize>(indexHeader.data().size()));
        }

        file.write(reinterpret_cast<const char*>(indexA.data()), static_cast<std::streamsize>(indexA.size()));
        file.write(reinterpret_cast<const char*>(indexB.data()), static_cast<std::streamsize>(indexB.size()));
        file.write(reinterpret_cast<const char*>(indexC.data()), static_cast<std::streamsize>(indexC.size()));
        file.write(reinterpret_cast<const char*>(indexD.data()), static_cast<std::streamsize>(indexD.size()));

        if (!file)
            return std::unexpected(std::format("Failed to write: {}", filePath.string()));

        return {};
    }


    size_t KeystoreWriter::size() const noexcept
    {
        return entries_.size();
    }


    bool KeystoreWriter::empty() const noexcept
    {
        return entries_.empty();
    }


    std::vector<KeystoreWriter::Entry> KeystoreWriter::collectEntries() const
    {
        std::vector<Entry> result;
        result.reserve(entries_.size());

        for (const auto& [key, pages] : entries_)
        {
            auto sorted = pages;
            std::ranges::sort(sorted);
            result.push_back({.key = key, .pages = std::move(sorted)});
        }

        std::ranges::sort(result, [](const Entry& a, const Entry& b) {
            return detail::unicode::toUtf32(a.key) < detail::unicode::toUtf32(b.key);
        });
        return result;
    }


    std::vector<uint8_t> KeystoreWriter::encodePages(const std::vector<PageReference>& pages)
    {
        detail::BinaryBuffer buf;

        // uint16_t LE entry count
        buf.writeLE(static_cast<uint16_t>(pages.size()));

        for (const auto& [page, item] : pages)
        {
            uint8_t flags = 0;

            // determine page encoding width
            size_t pageBytes = 0;
            if (page > 0)
            {
                if (page <= 0xFF)
                {
                    flags |= 0x01;
                    pageBytes = 1;
                }
                else if (page <= 0xFFFF)
                {
                    flags |= 0x02;
                    pageBytes = 2;
                }
                else
                {
                    flags |= 0x04;
                    pageBytes = 3;
                }
            }

            // determine item encoding width
            size_t itemBytes = 0;
            if (item > 0)
            {
                if (item <= 0xFF)
                {
                    flags |= 0x10;
                    itemBytes = 1;
                }
                else
                {
                    flags |= 0x20;
                    itemBytes = 2;
                }
            }

            buf.writeByte(flags);

            if (pageBytes == 1)
            {
                buf.writeByte(static_cast<uint8_t>(page));
            }
            else if (pageBytes == 2)
            {
                buf.writeByte(static_cast<uint8_t>(page >> 8));
                buf.writeByte(static_cast<uint8_t>(page));
            }
            else if (pageBytes == 3)
            {
                buf.writeByte(static_cast<uint8_t>(page >> 16));
                buf.writeByte(static_cast<uint8_t>(page >> 8));
                buf.writeByte(static_cast<uint8_t>(page));
            }

            if (itemBytes == 1)
            {
                buf.writeByte(static_cast<uint8_t>(item));
            }
            else if (itemBytes == 2)
            {
                buf.writeByte(static_cast<uint8_t>(item >> 8));
                buf.writeByte(static_cast<uint8_t>(item));
            }

            // skip extra and type fields
        }

        return buf.take();
    }


    Result<KeystoreWriter::WordsSection> KeystoreWriter::buildWordsSection(const std::vector<Entry>& entries)
    {
        std::vector<std::vector<uint8_t> > encodedPages;
        encodedPages.reserve(entries.size());
        for (const auto& [key, pages] : entries)
            encodedPages.push_back(encodePages(pages));

        // calculate offsets first
        WordsSection result;
        result.entryOffsets.reserve(entries.size());

        size_t cursor = 0;
        struct EntryLayout
        {
            size_t offset;
            size_t prefixLen;
            size_t pagesLen;
        };
        std::vector<EntryLayout> layouts;
        layouts.reserve(entries.size());

        for (size_t i = 0; i < entries.size(); ++i)
        {
            const size_t entryOffset = cursor;
            // 4 (pages_offset) + 1 (separator) + key.size() + 1 (null)
            const size_t prefixLen = 4 + 1 + entries[i].key.size() + 1;
            const size_t pagesLen = encodedPages[i].size();

            layouts.push_back({entryOffset, prefixLen, pagesLen});
            cursor += prefixLen + pagesLen;
        }

        result.data.reserve(cursor);
        detail::BinaryBuffer buf(cursor);

        for (size_t i = 0; i < entries.size(); ++i)
        {
            result.entryOffsets.push_back(static_cast<uint32_t>(layouts[i].offset));

            // points to the encoded pages within this entry
            const auto pagesOffset = static_cast<uint32_t>(layouts[i].offset + layouts[i].prefixLen);
            buf.writeLE(pagesOffset);

            // separator
            buf.writeByte(0x00);

            // null-terminated key
            buf.writeBytes(std::span(reinterpret_cast<const uint8_t*>(entries[i].key.data()), entries[i].key.size()));
            buf.writeByte(0x00);

            buf.writeBytes(encodedPages[i]);
        }

        result.data = buf.take();
        return result;
    }


    KeystoreWriter::IndexArrays KeystoreWriter::buildIndexArrays(const std::vector<Entry>& entries,
                                                                 const WordsSection& words)
    {
        const size_t n = entries.size();

        std::vector<size_t> indices(n);
        std::iota(indices.begin(), indices.end(), 0);

        IndexArrays result;

        auto toOffsets = [&](const std::vector<size_t>& order) {
            std::vector<uint32_t> offsets;
            offsets.reserve(order.size());
            for (const size_t i : order)
                offsets.push_back(words.entryOffsets[i]);
            return offsets;
        };

        std::vector<std::u32string> normalized(n);
        for (size_t i = 0; i < n; ++i)
            normalized[i] = detail::normalizeForPrefixCompare(entries[i].key);

        // Index A sorted by codepoint count, then case-folded comparison
        {
            auto order = indices;
            std::ranges::sort(order, [&](const size_t a, const size_t b) {
                const size_t lenA = detail::unicode::utf8CodepointCount(entries[a].key);
                const size_t lenB = detail::unicode::utf8CodepointCount(entries[b].key);
                if (lenA != lenB)
                    return lenA < lenB;

                return normalized[a] < normalized[b]; // same case-folded, hyphen/space-skipping comparison
            });
            result.length = toOffsets(order);
        }

        // Index B prefix case-insensitive, skip spaces/hyphens
        {
            auto order = indices;
            std::ranges::sort(order, [&](const size_t a, const size_t b) {
                return normalized[a] < normalized[b];
            });
            result.prefix = toOffsets(order);
        }

        // Index C suffix reversed key comparison
        {
            for (size_t i = 0; i < n; ++i)
                std::ranges::reverse(normalized[i]);

            auto order = indices;
            std::ranges::sort(order, [&](const size_t a, const size_t b) {
                return normalized[a] < normalized[b];
            });
            result.suffix = toOffsets(order);
        }

        // Index D same as prefix for now
        result.other = result.prefix;

        return result;
    }


    std::vector<uint8_t> KeystoreWriter::serializeIndexArray(const std::vector<uint32_t>& offsets)
    {
        detail::BinaryBuffer buf(sizeof(uint32_t) + offsets.size() * sizeof(uint32_t));
        buf.writeLE(static_cast<uint32_t>(offsets.size()));

        for (const auto offset : offsets)
            buf.writeLE(offset);

        return buf.take();
    }
}

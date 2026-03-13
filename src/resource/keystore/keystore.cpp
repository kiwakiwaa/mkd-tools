//
// kiwakiwaaにより 2026/02/11 に作成されました。
//

#include "MKD/resource/keystore.hpp"
#include "platform/mmap_file.hpp"

#include <array>
#include <bit>
#include <cstring>
#include <format>
#include <span>

static_assert(std::endian::native == std::endian::little, "Keystore assumes little-endian byte order");

namespace MKD
{
    namespace
    {
        struct FileHeader
        {
            uint32_t version;
            uint32_t magic1;
            uint32_t wordsOffset;
            uint32_t indexOffset;
        };

        struct FileHeaderV2Ext
        {
            uint32_t convTableOffset;
            uint32_t magic5;
            uint32_t magic6;
            uint32_t magic7;
        };

        struct IndexSectionHeader
        {
            uint32_t magic; // must be 0x04
            uint32_t offsetA;
            uint32_t offsetB;
            uint32_t offsetC;
            uint32_t offsetD;
        };

        static_assert(sizeof(FileHeader) == 16);
        static_assert(sizeof(FileHeaderV2Ext) == 16);
        static_assert(sizeof(IndexSectionHeader) == 20);


        template<typename T>
        const T* ptrAt(const MappedFile& file, const size_t offset)
        {
            if (offset + sizeof(T) > file.size()) return nullptr;
            return reinterpret_cast<const T*>(file.data().data() + offset);
        }


        Result<std::span<const uint32_t>> parseIndex(
            const MappedFile& file,
            const size_t sectionBase,
            const uint32_t start,
            const uint32_t end)
        {
            if (start == 0)
                return std::span<const uint32_t>{};

            if (end <= start)
                return std::unexpected("Invalid index range");

            const size_t bytes = end - start;
            if (bytes < 4 || bytes % 4 != 0)
                return std::unexpected("Invalid index size");

            auto span = file.slice(sectionBase + start, bytes);
            if (!span) return std::unexpected(span.error());

            const auto* data = reinterpret_cast<const uint32_t*>(span->data());
            const size_t total = bytes / sizeof(uint32_t);

            if (data[0] + 1 != total)
                return std::unexpected(std::format(
                    "Index count mismatch: stored {} but have {} offsets",
                    data[0], total - 1));

            return std::span(data + 1, total - 1);
        }
    }


    struct ConversionEntry
    {
        uint32_t page;
        uint16_t item;
        uint16_t padding;
    };

    static_assert(sizeof(ConversionEntry) == 8);

    struct Keystore::Impl
    {
        MappedFile file;
        std::string dictId;
        std::string filename;
        size_t wordsOffset = 0;
        std::span<const uint32_t> indices[4] = {};
        std::span<const ConversionEntry> convTable;

        Impl(MappedFile mappedFile,
             std::string dict,
             std::string name,
             const size_t words,
             std::span<const uint32_t> index0,
             std::span<const uint32_t> index1,
             std::span<const uint32_t> index2,
             std::span<const uint32_t> index3,
             std::span<const ConversionEntry> conversions) noexcept
            : file(std::move(mappedFile))
              , dictId(std::move(dict))
              , filename(std::move(name))
              , wordsOffset(words)
              , indices{index0, index1, index2, index3}
              , convTable(conversions)
        {
        }

        struct WordEntry
        {
            std::string_view key;
            size_t pagesOffset;
        };

        [[nodiscard]] Result<uint32_t> wordOffset(KeystoreIndex type, size_t index) const
        {
            const auto i = static_cast<size_t>(type);
            if (i >= 4)
                return std::unexpected("Invalid index type");

            const auto& arr = indices[i];
            if (arr.empty())
                return std::unexpected("Index not present");
            if (index >= arr.size())
                return std::unexpected(std::format(
                    "Index out of bounds: {} >= {}", index, arr.size()));

            return arr[index];
        }

        [[nodiscard]] Result<WordEntry> parseWordEntry(const uint32_t offset) const
        {
            const size_t abs = wordsOffset + offset;

            if (abs + 6 > file.size())
                return std::unexpected("Word offset out of bounds");

            const uint8_t* ptr = file.data().data() + abs;

            uint32_t pagesOffset;
            std::memcpy(&pagesOffset, ptr, sizeof(uint32_t));

            const auto* strBegin = reinterpret_cast<const char*>(ptr + 5);
            const size_t maxLen = file.size() - (abs + 5);

            const auto* nullPos = static_cast<const char*>(std::memchr(strBegin, 0, maxLen));
            if (!nullPos)
                return std::unexpected("Unterminated word string");

            return WordEntry{
                .key = std::string_view(strBegin, static_cast<size_t>(nullPos - strBegin)),
                .pagesOffset = pagesOffset,
            };
        }

        [[nodiscard]] Result<std::vector<EntryId>> decodeEntryIds(const size_t pagesOffset) const
        {
            const size_t abs = wordsOffset + pagesOffset;

            if (abs + 2 > file.size())
                return std::unexpected(std::format(
                    "Pages offset out of bounds: {} + 2 > {}", abs, file.size()));

            return MKD::decodeEntryIds(file.data().subspan(abs));
        }

        [[nodiscard]] bool needsConversion() const noexcept
        {
            return !convTable.empty()
                   && (dictId == "KNEJ.EJ" || dictId == "KNJE.JE");
        }

        void applyConversion(std::vector<EntryId>& refs) const noexcept
        {
            for (auto& [page, entry] : refs)
            {
                if (page < convTable.size())
                {
                    const auto& mapped = convTable[page];
                    page = mapped.page;
                    entry = mapped.item;
                }
            }
        }
    };


    Keystore::Keystore(std::unique_ptr<Impl> impl) noexcept
        : impl_(std::move(impl))
    {
    }

    Keystore::~Keystore() = default;

    Keystore::Keystore(Keystore&&) noexcept = default;

    Keystore& Keystore::operator=(Keystore&&) noexcept = default;


    Result<Keystore> Keystore::open(const std::filesystem::path& path, std::string dictId)
    {
        auto file = MappedFile::open(path);
        if (!file) return std::unexpected(file.error());

        const size_t fileSize = file->size();

        const auto* hdr = ptrAt<FileHeader>(*file, 0);
        if (!hdr)
            return std::unexpected("File too small for header");

        if (hdr->version != KEYSTORE_V1 && hdr->version != KEYSTORE_V2)
            return std::unexpected(std::format("Invalid keystore version: 0x{:x}", hdr->version));

        if (hdr->magic1 != 0)
            return std::unexpected("Invalid magic1 field");

        if (hdr->wordsOffset >= hdr->indexOffset)
            return std::unexpected("Invalid offset ordering: wordsOffset >= indexOffset");

        uint32_t convTableOffset = 0;

        if (hdr->version == KEYSTORE_V2)
        {
            const auto* ext = ptrAt<FileHeaderV2Ext>(*file, sizeof(FileHeader));
            if (!ext)
                return std::unexpected("File too small for v2 header");

            if (ext->magic5 != 0 || ext->magic6 != 0 || ext->magic7 != 0)
                return std::unexpected("Invalid magic fields in v2 header");

            convTableOffset = ext->convTableOffset;

            if (convTableOffset != 0 && hdr->indexOffset >= convTableOffset)
                return std::unexpected("Invalid conversion table offset");
        }

        const size_t indexBase = hdr->indexOffset;
        const size_t indexEnd = convTableOffset != 0 ? convTableOffset : fileSize;

        const auto* idxHdr = ptrAt<IndexSectionHeader>(*file, indexBase);
        if (!idxHdr)
            return std::unexpected("Index section header truncated");

        if (idxHdr->magic != 0x04)
            return std::unexpected(std::format("Invalid index magic: expected 0x04, got 0x{:x}",
                                               idxHdr->magic));

        const auto sectionSize = static_cast<uint32_t>(indexEnd - indexBase);
        const std::array offsets = {
            idxHdr->offsetA, idxHdr->offsetB,
            idxHdr->offsetC, idxHdr->offsetD, sectionSize
        };

        auto endOf = [&](size_t i) -> uint32_t {
            for (size_t j = i + 1; j < offsets.size(); ++j)
                if (offsets[j] != 0) return offsets[j];
            return sectionSize;
        };

        static constexpr const char* indexNames[] = {"A", "B", "C", "D"};
        std::span<const uint32_t> indices[4];

        for (size_t i = 0; i < 4; ++i)
        {
            auto idx = parseIndex(*file, indexBase, offsets[i], endOf(i));
            if (!idx)
                return std::unexpected(std::format("Index {}: {}", indexNames[i], idx.error()));
            indices[i] = *idx;
        }

        std::span<const ConversionEntry> convTable;

        if (convTableOffset != 0)
        {
            const auto* countPtr = ptrAt<uint32_t>(*file, convTableOffset);
            if (!countPtr)
                return std::unexpected("Conversion table header truncated");

            const uint32_t count = *countPtr;
            const size_t required = sizeof(uint32_t) + static_cast<size_t>(count) * sizeof(ConversionEntry);

            if (convTableOffset + required > fileSize)
                return std::unexpected("Conversion table extends past end of file");

            const auto* entries = reinterpret_cast<const ConversionEntry*>(
                file->data().data() + convTableOffset + sizeof(uint32_t));
            convTable = std::span(entries, count);
        }

        auto impl = std::make_unique<Impl>(
            std::move(*file),
            std::move(dictId),
            path.filename().string(),
            hdr->wordsOffset,
            indices[0],
            indices[1],
            indices[2],
            indices[3],
            convTable);


        return Keystore(std::move(impl));
    }


    Result<KeystoreEntry> Keystore::entryAt(const KeystoreIndex type, const size_t index) const
    {
        auto off = impl_->wordOffset(type, index);
        if (!off) return std::unexpected(off.error());

        auto entry = impl_->parseWordEntry(*off);
        if (!entry) return std::unexpected(entry.error());

        return KeystoreEntry{
            .key = entry->key,
            .index = index,
        };
    }

    Result<std::string_view> Keystore::keyAt(const KeystoreIndex type, const size_t index) const
    {
        auto entry = entryAt(type, index);
        if (!entry) return std::unexpected(entry.error());
        return entry->key;
    }

    Result<std::vector<EntryId>> Keystore::entryIdsAt(const KeystoreIndex type, const size_t index) const
    {
        auto off = impl_->wordOffset(type, index);
        if (!off) return std::unexpected(off.error());

        auto entry = impl_->parseWordEntry(*off);
        if (!entry) return std::unexpected(entry.error());

        auto entryIds = impl_->decodeEntryIds(entry->pagesOffset);
        if (!entryIds) return std::unexpected(entryIds.error());

        if (impl_->needsConversion())
            impl_->applyConversion(*entryIds);

        return entryIds;
    }


    size_t Keystore::indexSize(const KeystoreIndex type) const noexcept
    {
        const auto i = static_cast<size_t>(type);
        return i < 4 ? impl_->indices[i].size() : 0;
    }


    std::string_view Keystore::filename() const noexcept { return impl_->filename; }

    Keystore::Iterator Keystore::begin(const KeystoreIndex type) const noexcept
    {
        return Iterator{this, type, 0};
    }

    Keystore::Iterator Keystore::end(const KeystoreIndex type) const noexcept
    {
        return Iterator{this, type, indexSize(type)};
    }

    Keystore::Iterator::Iterator(const Keystore* keystore, const KeystoreIndex type, const size_t index) noexcept
        : keystore_(keystore), type_(type), index_(index)
    {
    }

    Keystore::Iterator::value_type Keystore::Iterator::operator*() const
    {
        if (keystore_ == nullptr)
            return {};

        auto entry = keystore_->entryAt(type_, index_);
        if (!entry)
            return {};

        return *entry;
    }

    Keystore::Iterator& Keystore::Iterator::operator++()
    {
        ++index_;
        return *this;
    }

    Keystore::Iterator Keystore::Iterator::operator++(int)
    {
        auto copy = *this;
        ++(*this);
        return copy;
    }
}

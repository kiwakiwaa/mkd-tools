//
// kiwakiwaaにより 2026/02/12 に作成されました。
//

#include "MKD/resource/headline_store.hpp"
#include "platform/mmap_file.hpp"

#include <utf8.h>

#include <algorithm>
#include <bit>
#include <cstring>
#include <format>
#include <ranges>
#include <span>

namespace MKD
{
    static_assert(std::endian::native == std::endian::little, "HeadlineStore assumes little-endian byte order");

    namespace
    {
        template<typename T>
        const T* ptrAt(const MappedFile& file, const size_t offset)
        {
            if (offset + sizeof(T) > file.size())
                return nullptr;
            return reinterpret_cast<const T*>(file.data().data() + offset);
        }
    }

    uint32_t HeadlineHeader::effectiveStride() const noexcept
    {
        return recordStride != 0 ? recordStride : HEADLINE_DEFAULT_STRIDE;
    }


    std::u16string HeadlineComponents::full() const
    {
        std::u16string result;
        result.reserve(prefix.size() + headline.size() + suffix.size());

        if (!prefix.empty())
            result.append(prefix);

        if (!headline.empty())
            result.append(headline);

        if (!suffix.empty())
            result.append(suffix);

        return result;
    }


    std::string HeadlineComponents::fullUtf8() const
    {
        return prefixUtf8() + headlineUtf8() + suffixUtf8();
    }


    std::string HeadlineComponents::prefixUtf8() const
    {
#if defined(_WIN32)
        std::string result;
        utf8::utf16to8(prefix.begin(), prefix.end(), std::back_inserter(result));
        return result;
#else
        return utf8::utf16to8(prefix);
#endif
    }


    std::string HeadlineComponents::headlineUtf8() const
    {
#if defined(_WIN32)
        std::string result;
        utf8::utf16to8(headline.begin(), headline.end(), std::back_inserter(result));
        return result;
#else
        return utf8::utf16to8(headline);
#endif
    }


    std::string HeadlineComponents::suffixUtf8() const
    {
#if defined(_WIN32)
        std::string result;
        utf8::utf16to8(suffix.begin(), suffix.end(), std::back_inserter(result));
        return result;
#else
        return utf8::utf16to8(suffix);
#endif
    }


    Result<HeadlineStore> HeadlineStore::load(const fs::path& filePath)
    {
        auto mapped = MappedFile::open(filePath);
        if (!mapped)
            return std::unexpected(std::format("Failed to open headline store '{}'", filePath.string()));

        const auto* header = ptrAt<HeadlineHeader>(*mapped, 0);
        if (!header)
            return std::unexpected("Headline store file too small for header");

        const uint32_t stride = header->effectiveStride();
        const uint32_t entryCount = header->entryCount;
        const size_t recordsOffset = header->recordsOffset;
        const size_t stringsOffset = header->stringsOffset;
        const size_t recordsBytes = static_cast<size_t>(entryCount) * stride;

        if (stride < sizeof(HeadlineRecord))
            return std::unexpected(std::format(
                "Headline record stride {} is smaller than record size {}",
                stride, sizeof(HeadlineRecord)));

        if (recordsOffset < sizeof(HeadlineHeader) || recordsOffset > stringsOffset)
            return std::unexpected("Headline records offset out of bounds");

        if (stringsOffset < sizeof(HeadlineHeader) || stringsOffset > mapped->size())
            return std::unexpected("Headline strings offset out of bounds");

        if (recordsOffset > stringsOffset)
            return std::unexpected("Headline records offset must not exceed strings offset");

        if (recordsBytes > mapped->size() - recordsOffset)
            return std::unexpected("Headline records section exceeds file size");

        if (stringsOffset < recordsOffset + recordsBytes)
            return std::unexpected("Headline strings section overlaps records section");

        return HeadlineStore(
            std::move(*mapped),
            filePath.filename().string(),
            entryCount,
            stride,
            recordsOffset,
            stringsOffset);
    }

    Result<HeadlineComponents> HeadlineStore::operator[](const size_t index) const
    {
        if (index >= entryCount_)
            return std::unexpected(std::format(
                "Headline index {} out of range (size = {})", index, entryCount_));

        return componentsFromRecord(recordAt(index));
    }


    Result<EntryId> HeadlineStore::entryIdAt(const size_t index) const
    {
        if (index >= entryCount_)
            return std::unexpected(std::format(
                "Headline index {} out of range (size = {})", index, entryCount_));

        HeadlineRecord rec{};
        std::memcpy(&rec, recordAt(index), sizeof(rec));
        return rec.entryId();
    }


    size_t HeadlineStore::size() const noexcept
    {
        return entryCount_;
    }


    bool HeadlineStore::empty() const noexcept
    {
        return entryCount_ == 0;
    }


    std::string_view HeadlineStore::filename() const
    {
        return filename_;
    }


    HeadlineStore::HeadlineStore(MappedFile&& fileData,
                                 std::string filename,
                                 const uint32_t entryCount,
                                 const uint32_t stride,
                                 const size_t recordsOffset,
                                 const size_t stringsOffset)
        : fileData_(std::move(fileData))
          , filename_(std::move(filename))
          , entryCount_(entryCount)
          , stride_(stride)
          , recordsOffset_(recordsOffset)
          , stringsOffset_(stringsOffset)
    {
    }


    HeadlineStore::Iterator::Iterator(const HeadlineStore* store, const size_t pos) noexcept
        : store_(store)
          , position_(pos)
    {
    }

    HeadlineStore::Iterator::value_type HeadlineStore::Iterator::operator*() const
    {
        auto result = (*store_)[position_];
        if (!result)
            throw std::out_of_range(result.error());
        return *result;
    }

    HeadlineStore::Iterator::value_type HeadlineStore::Iterator::operator[](const difference_type n) const
    {
        auto result = (*store_)[position_ + n];
        if (!result)
            throw std::out_of_range(result.error());
        return *result;
    }

    HeadlineStore::Iterator& HeadlineStore::Iterator::operator++() noexcept
    {
        ++position_;
        return *this;
    }

    HeadlineStore::Iterator HeadlineStore::Iterator::operator++(int) noexcept
    {
        const auto copy = *this;
        ++position_;
        return copy;
    }

    HeadlineStore::Iterator& HeadlineStore::Iterator::operator--() noexcept
    {
        --position_;
        return *this;
    }

    HeadlineStore::Iterator HeadlineStore::Iterator::operator--(int) noexcept
    {
        const auto copy = *this;
        --position_;
        return copy;
    }

    HeadlineStore::Iterator& HeadlineStore::Iterator::operator+=(const difference_type n) noexcept
    {
        position_ += n;
        return *this;
    }

    HeadlineStore::Iterator& HeadlineStore::Iterator::operator-=(const difference_type n) noexcept
    {
        position_ -= n;
        return *this;
    }

    HeadlineStore::Iterator HeadlineStore::Iterator::operator+(const difference_type n) const noexcept
    {
        return {store_, position_ + n};
    }

    HeadlineStore::Iterator HeadlineStore::Iterator::operator-(const difference_type n) const noexcept
    {
        return {store_, position_ - n};
    }

    HeadlineStore::Iterator operator+(const HeadlineStore::Iterator::difference_type n,
                                      const HeadlineStore::Iterator& it) noexcept
    {
        return it + n;
    }

    HeadlineStore::Iterator::difference_type HeadlineStore::Iterator::operator-(const Iterator& other) const noexcept
    {
        return static_cast<difference_type>(position_) - static_cast<difference_type>(other.position_);
    }

    HeadlineStore::Iterator HeadlineStore::begin() const noexcept
    {
        return {this, 0};
    }

    HeadlineStore::Iterator HeadlineStore::end() const noexcept
    {
        return {this, entryCount_};
    }


    const uint8_t* HeadlineStore::recordAt(const size_t index) const noexcept
    {
        return fileData_.data().data() + recordsOffset_ + index * stride_;
    }

    Result<std::u16string_view> HeadlineStore::stringAt(const uint32_t offset) const
    {
        if (offset % sizeof(char16_t) != 0)
            return std::unexpected(std::format("Headline string offset {} is not UTF-16 aligned", offset));

        const size_t absoluteOffset = stringsOffset_ + offset;
        if (absoluteOffset >= fileData_.size())
            return std::unexpected(std::format("Headline string offset {} out of bounds", offset));

        const auto stringBytes = fileData_.data().subspan(absoluteOffset);
        if (stringBytes.size() < sizeof(char16_t))
            return std::unexpected(std::format("Headline string offset {} has no terminator", offset));

        const auto* str = reinterpret_cast<const char16_t*>(stringBytes.data());
        const size_t units = stringBytes.size() / sizeof(char16_t);

        for (size_t len = 0; len < units; ++len)
        {
            if (str[len] == u'\0')
                return std::u16string_view(str, len);
        }

        return std::unexpected(std::format("Headline string at offset {} is not null-terminated", offset));
    }


    Result<HeadlineComponents> HeadlineStore::componentsFromRecord(const uint8_t* record) const
    {
        HeadlineRecord rec{};
        std::memcpy(&rec, record, sizeof(rec));

        auto headline = stringAt(rec.headlineOffset);
        if (!headline)
            return std::unexpected(headline.error());

        std::u16string_view prefix;
        if (rec.prefixOffset != 0)
        {
            auto p = stringAt(rec.prefixOffset);
            if (!p) return std::unexpected(p.error());
            prefix = *p;
        }

        std::u16string_view suffix;
        if (rec.suffixOffset != 0)
        {
            auto s = stringAt(rec.suffixOffset);
            if (!s) return std::unexpected(s.error());
            suffix = *s;
        }

        return HeadlineComponents{
            .prefix = prefix,
            .headline = *headline,
            .suffix = suffix,
            .entryId = rec.entryId(),
        };
    }
}

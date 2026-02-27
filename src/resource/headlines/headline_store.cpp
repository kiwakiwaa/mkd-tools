//
// kiwakiwaaにより 2026/02/12 に作成されました。
//

#include "MKD/resource/headline/headline_store.hpp"
#include "MKD/platform/binary_file_reader.hpp"

#include <utf8.h>

#include <algorithm>
#include <format>
#include <ranges>

namespace MKD
{
    void HeadlineHeader::swapEndianness() noexcept
    {
        entryCount = std::byteswap(entryCount);
        recordsOffset = std::byteswap(recordsOffset);
        stringsOffset = std::byteswap(stringsOffset);
        recordStride = std::byteswap(recordStride);
    }

    uint32_t HeadlineHeader::effectiveStride() const noexcept
    {
        return recordStride != 0 ? recordStride : HEADLINE_DEFAULT_STRIDE;
    }


    std::u16string HeadlineComponents::full() const
    {
        std::u16string result(prefix.size() + headline.size(), suffix.size());

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
        return utf8::utf16to8(prefix);
    }


    std::string HeadlineComponents::headlineUtf8() const
    {
        return utf8::utf16to8(headline);
    }


    std::string HeadlineComponents::suffixUtf8() const
    {
        return utf8::utf16to8(suffix);
    }


    std::expected<HeadlineStore, std::string> HeadlineStore::load(const fs::path& filePath)
    {
        auto reader = BinaryFileReader::open(filePath);
        if (!reader)
            return std::unexpected(std::format("Failed to open headline store '{}'", filePath.string()));

        const auto fileSize = reader->remainingBytes();
        if (fileSize < sizeof(HeadlineHeader))
            return std::unexpected("Headline store file too small for header");

        std::vector<uint8_t> data(fileSize);
        if (auto readResult = reader->readBytes(std::span(data)); !readResult)
            return std::unexpected("Failed to read headline store");

        if (auto seekResult = reader->seek(0); !seekResult)
            return std::unexpected("Failed to seek to beginning of headline store");

        auto header = reader->readStruct<HeadlineHeader>();
        if (!header)
            return std::unexpected("Failed to read headline header");

        const uint32_t stride = header->effectiveStride();
        const uint32_t entryCount = header->entryCount;
        const uint8_t* base = data.data();
        const uint8_t* records = base + header->recordsOffset;
        const uint8_t* strings = base + header->stringsOffset;

        return HeadlineStore(std::move(data), filePath.filename().string(), entryCount, stride, records, strings);
    }

    std::expected<HeadlineComponents, std::string> HeadlineStore::operator[](const size_t index) const
    {
        if (index >= entryCount_)
            return std::unexpected(std::format(
                "Headline index {} out of range (size = {})", index, entryCount_));

        return componentsFromRecord(recordAt(index));
    }


    std::expected<HeadlineEntryId, std::string> HeadlineStore::entryIdAt(const size_t index) const
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


    HeadlineStore::HeadlineStore(std::vector<uint8_t>&& fileData,
                                 std::string filename,
                                 const uint32_t entryCount,
                                 const uint32_t stride,
                                 const uint8_t* records,
                                 const uint8_t* strings)
        : fileData_(std::move(fileData))
          , filename_(std::move(filename))
          , entryCount_(entryCount)
          , stride_(stride)
          , records_(records)
          , strings_(strings)
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
        return records_ + index * stride_;
    }

    std::expected<std::u16string_view, std::string> HeadlineStore::stringAt(const uint32_t offset) const
    {
        const auto* str = reinterpret_cast<const char16_t*>(strings_ + offset);

        size_t len = 0;
        while (str[len] != u'\0')
            ++len;

        return std::u16string_view(str, len);
    }


    std::expected<HeadlineComponents, std::string> HeadlineStore::componentsFromRecord(const uint8_t* record) const
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

//
// Caoimheにより 2026/01/16 に作成されました。
//

#include "monokakido/resource/nrsc/nrsc_index.hpp"
#include "monokakido/core/platform/fs.hpp"

#include <cassert>
#include <bit>
#include <format>
#include <fstream>

namespace monokakido::resource
{
    void IndexHeader::swapEndianness() noexcept
    {
        zeroField = std::byteswap(zeroField);
        recordCount = std::byteswap(recordCount);
    }


    std::expected<NrscIndex, std::string> NrscIndex::load(const fs::path& directoryPath)
    {
        auto filePathResult = platform::fs::getValidatedFilePath(directoryPath, "index.nidx");
        if (!filePathResult)
            return std::unexpected(filePathResult.error());

        auto reader = platform::fs::BinaryFileReader::open(*filePathResult);
        if (!reader)
            return std::unexpected(reader.error());

        auto header = reader->readStruct<IndexHeader>();
        if (!header)
            return std::unexpected(header.error());

        auto records = reader->readStructArray<NrscIndexRecord>(header->recordCount);
        if (!records)
            return std::unexpected(records.error());

        // Read remaining bytes as ID strings
        const size_t stringRegionSize = reader->remainingBytes();
        auto idStrings = reader->readBytesIntoString(stringRegionSize);
        if (!idStrings)
            return std::unexpected(idStrings.error());

        const size_t totalHeaderSize = HEADER_SIZE + header->recordCount * RECORD_SIZE;

        return NrscIndex(
            std::move(*records),
            std::move(*idStrings),
            totalHeaderSize
        );
    }


    std::expected<NrscIndexRecord, std::string> NrscIndex::findById(std::string_view id) const
    {
        const auto it = std::lower_bound(records_.begin(), records_.end(), id,
                                         [this](const NrscIndexRecord& record, std::string_view searchId) {
                                             const auto idResult = this->getIdAt(record.idOffset());
                                             if (!idResult)
                                                 return false;

                                             return *idResult < searchId;
                                         });

        if (it == records_.end())
            return std::unexpected(std::format("Resource not found: {}", id));

        auto foundIdResult = getIdAt(it->idOffset());
        if (!foundIdResult)
            return std::unexpected(foundIdResult.error());

        if (*foundIdResult != id)
            return std::unexpected(std::format("Resource not found: {}", id));

        return *it;
    }


    std::expected<std::pair<std::string_view, NrscIndexRecord>, std::string> NrscIndex::getByIndex(
        const size_t index) const
    {
        if (index >= records_.size())
            return std::unexpected(std::format("Index {} out of range (size: {})", index, records_.size()));

        const auto& record = records_.at(index);
        auto idResult = getIdAt(record.idOffset());
        if (!idResult)
            return std::unexpected(idResult.error());

        return std::make_pair(*idResult, record);
    }


    size_t NrscIndex::size() const noexcept
    {
        return records_.size();
    }


    bool NrscIndex::empty() const noexcept
    {
        return size() == 0;
    }


    NrscIndex::Iterator::Iterator(const NrscIndex* index, const size_t pos)
        : index_(index), position_(pos)
    {
    }

    NrscIndex::Iterator::value_type NrscIndex::Iterator::operator*() const
    {
        assert(index_ != nullptr && "Dereferencing invalid iterator");
        assert(position_ < index_->size() && "Dereferencing end iterator");

        auto result = index_->getByIndex(position_);
        if (!result)
        {
            throw std::runtime_error(
                std::format("Index iteration failed at position {}: {}",
                            position_, result.error()));
        }

        return *result;
    }

    NrscIndex::Iterator::value_type NrscIndex::Iterator::operator[](difference_type n) const
    {
        return *(*this + n);
    }


    NrscIndex::Iterator& NrscIndex::Iterator::operator++()
    {
        ++position_;
        return *this;
    }

    NrscIndex::Iterator NrscIndex::Iterator::operator++(int)
    {
        const auto temp = *this;
        ++*this;
        return temp;
    }

    NrscIndex::Iterator& NrscIndex::Iterator::operator--()
    {
        --position_;
        return *this;
    }

    NrscIndex::Iterator NrscIndex::Iterator::operator--(int)
    {
        const auto temp = *this;
        --*this;
        return temp;
    }

    // Arithmetic
    NrscIndex::Iterator& NrscIndex::Iterator::operator+=(const difference_type n)
    {
        position_ += n;
        return *this;
    }

    NrscIndex::Iterator& NrscIndex::Iterator::operator-=(const difference_type n)
    {
        position_ -= n;
        return *this;
    }

    NrscIndex::Iterator NrscIndex::Iterator::operator+(const difference_type n) const
    {
        auto temp = *this;
        temp += n;
        return temp;
    }

    NrscIndex::Iterator NrscIndex::Iterator::operator-(const difference_type n) const
    {
        auto temp = *this;
        temp -= n;
        return temp;
    }

    NrscIndex::Iterator operator+(const NrscIndex::Iterator::difference_type n, const NrscIndex::Iterator& it)
    {
        return it + n;
    }

    NrscIndex::Iterator::difference_type NrscIndex::Iterator::operator-(const Iterator& other) const
    {
        return static_cast<difference_type>(position_) - static_cast<difference_type>(other.position_);
    }

    NrscIndex::Iterator NrscIndex::begin() const
    {
        return Iterator{this, 0};
    }

    NrscIndex::Iterator NrscIndex::end() const
    {
        return Iterator{this, size()};
    }


    NrscIndex::NrscIndex(std::vector<NrscIndexRecord>&& records, std::string&& idStrings, const size_t headerSize)
        : records_(std::move(records))
          , idStrings_(std::move(idStrings))
          , headerSize_(headerSize)
    {
    }


    std::expected<std::string_view, std::string> NrscIndex::getIdAt(size_t offset) const
    {
        const size_t adjustedOffset = offset - headerSize_;

        if (adjustedOffset > 0 && idStrings_.at(adjustedOffset - 1) != '\0')
            return std::unexpected(std::format("Invalid ID offset: {} (not at string boundary)", offset));

        if (adjustedOffset >= idStrings_.size())
            return std::unexpected(std::format("ID offset {} out of range", offset));

        // find the null terminator
        const auto nullPos = idStrings_.find('\0', adjustedOffset);
        if (nullPos == std::string::npos)
        {
            return std::unexpected(std::format("ID string at offset {} not null-terminated", offset));
        }

        const size_t length = nullPos - adjustedOffset;
        return std::string_view(idStrings_.data() + adjustedOffset, length);
    }


    std::expected<IndexHeader, std::string> NrscIndex::readHeader(std::ifstream& file)
    {
        IndexHeader header{};
        file.read(reinterpret_cast<char*>(&header), HEADER_SIZE);

        if (!file)
            return std::unexpected(platform::fs::makeFilestreamError(file, "read header"));

        // convert to native endianness if needed
        if constexpr (std::endian::native == std::endian::big)
            header.recordCount = std::byteswap(header.recordCount);

        return header;
    }


    std::expected<std::vector<NrscIndexRecord>, std::string> NrscIndex::readRecords(
        std::ifstream& file, const uint32_t count)
    {
        std::vector<NrscIndexRecord> records(count);

        const auto bytesToRead = static_cast<std::streamsize>(count * RECORD_SIZE);
        file.read(reinterpret_cast<char*>(records.data()), bytesToRead);

        if (!file)
            return std::unexpected(platform::fs::makeFilestreamError(file, "read records"));

        for (auto& record : records)
        {
            record.toLittleEndian();
        }

        return records;
    }


    std::expected<std::string, std::string> NrscIndex::readIdStrings(std::ifstream& file, const size_t stringRegionSize)
    {
        std::string idStrings;
        idStrings.resize(stringRegionSize);

        const auto bytesToRead = static_cast<std::streamsize>(stringRegionSize);
        file.read(idStrings.data(), bytesToRead);

        if (!file)
            return std::unexpected(platform::fs::makeFilestreamError(file, "read ID strings region"));

        return idStrings;
    }
}

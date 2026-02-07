//
// kiwakiwaaにより 2026/01/19 に作成されました。
//

#include "monokakido/resource/rsc/rsc_index.hpp"
#include "monokakido/core/platform/fs.hpp"

#include <bit>
#include <cassert>
#include <format>
#include <fstream>

namespace monokakido
{
    void IdxHeader::swapEndianness() noexcept
    {
        length = std::byteswap(length);
        padding = std::byteswap(padding);
    }


    std::expected<RscIndex, std::string> RscIndex::load(const fs::path& directoryPath)
    {
        auto idxResult = loadIdxFile(directoryPath);
        if (!idxResult)
            return std::unexpected(std::format("Failed to load rsc index at: {}\n{}", directoryPath.string(),
                                               idxResult.error()));

        auto mapResult = loadMapFile(directoryPath);
        if (!mapResult)
            return std::unexpected(std::format("Failed to load rsc map at: {}\n{}", directoryPath.string(),
                                               mapResult.error()));


        return RscIndex(std::move(*idxResult), std::move(*mapResult));
    }


    std::expected<MapRecord, std::string> RscIndex::findById(uint32_t itemId) const
    {
        // If no idx records, use direct indexing
        // Seems to only be the case for fonts
        if (!idxRecords_.has_value())
        {
            if (itemId >= mapRecords_.size())
                return std::unexpected(std::format("Item ID {} not found (out of range)", itemId));

            return mapRecords_[itemId];
        }

        const auto& idxRecords = *idxRecords_;

        // Try fast guesses first (IDs are often sequential)
        const size_t guessIdx = std::min(static_cast<size_t>(itemId), idxRecords.size() - 1);
        if (idxRecords[guessIdx].id() == itemId)
        {
            const size_t mapIdx = idxRecords[guessIdx].mapIndex();
            if (mapIdx >= mapRecords_.size())
                return std::unexpected(std::format("Index mismatch for item ID {}", itemId));
            return mapRecords_[mapIdx];
        }

        // Try itemId-1 as another common pattern
        if (itemId > 0)
        {
            const size_t guessIdx2 = std::min(static_cast<size_t>(itemId - 1), idxRecords.size() - 1);
            if (idxRecords[guessIdx2].id() == itemId)
            {
                const size_t mapIdx = idxRecords[guessIdx2].mapIndex();
                if (mapIdx >= mapRecords_.size())
                    return std::unexpected(std::format("Index mismatch for item ID {}", itemId));
                return mapRecords_[mapIdx];
            }
        }

        // Fall back to binary search
        const auto it = std::lower_bound(
            idxRecords.begin(),
            idxRecords.end(),
            itemId,
            [](const IdxRecord& record, const uint32_t id) { return record.id() < id; }
        );

        if (it == idxRecords.end() || it->id() != itemId)
            return std::unexpected(std::format("Item ID {} not found", itemId));

        const size_t mapIdx = it->mapIndex();
        if (mapIdx >= mapRecords_.size())
            return std::unexpected(std::format("Index mismatch for item ID {}", itemId));

        return mapRecords_[mapIdx];
    }


    std::expected<std::pair<uint32_t, MapRecord>, std::string> RscIndex::getByIndex(size_t index) const
    {
        if (index >= mapRecords_.size())
            return std::unexpected(std::format("Index {} out of range (size: {})", index, mapRecords_.size()));

        uint32_t itemId;

        if (idxRecords_.has_value())
        {
            const auto& idxRecords = *idxRecords_;

            // Validate that idx record exists and points to correct map index
            if (index >= idxRecords.size())
                return std::unexpected(std::format("Index {} out of range in idx records", index));

            const auto& idxRecord = idxRecords[index];

            itemId = idxRecord.id();
        }
        else
        {
            // Direct indexing mode: index == itemId
            itemId = static_cast<uint32_t>(index);
        }

        return std::pair{itemId, mapRecords_[index]};
    }


    size_t RscIndex::size() const noexcept
    {
        return mapRecords_.size();
    }


    bool RscIndex::empty() const noexcept
    {
        return mapRecords_.empty();
    }


    RscIndex::Iterator::Iterator(const RscIndex* index, size_t pos)
        : index_(index), position_(pos)
    {
    }


    RscIndex::RscIndex(std::optional<std::vector<IdxRecord>>&& idxRecords, std::vector<MapRecord>&& mapRecords)
        : idxRecords_(std::move(idxRecords)), mapRecords_(std::move(mapRecords))
    {
    }

    RscIndex::Iterator::value_type RscIndex::Iterator::operator*() const
    {
        assert(index_ != nullptr && "Dereferencing invalid iterator");
        assert(position_ < index_->size() && "Dereferencing end iterator");

        auto result = index_->getByIndex(position_);
        if (!result)
        {
            throw std::runtime_error(std::format("Index iteration failed at position {} : {}",
                                                 position_,
                                                 result.error())
            );
        }

        return *result;
    }

    RscIndex::Iterator::value_type RscIndex::Iterator::operator[](const difference_type n) const
    {
        return *(*this + n);
    }

    RscIndex::Iterator& RscIndex::Iterator::operator++()
    {
        ++position_;
        return *this;
    }

    RscIndex::Iterator RscIndex::Iterator::operator++(int)
    {
        const auto temp = *this;
        ++*this;
        return temp;
    }

    RscIndex::Iterator& RscIndex::Iterator::operator--()
    {
        --position_;
        return *this;
    }

    RscIndex::Iterator RscIndex::Iterator::operator--(int)
    {
        const auto temp = *this;
        --*this;
        return temp;
    }

    RscIndex::Iterator& RscIndex::Iterator::operator+=(const difference_type n)
    {
        position_ += n;
        return *this;
    }

    RscIndex::Iterator& RscIndex::Iterator::operator-=(const difference_type n)
    {
        position_ -= n;
        return *this;
    }

    RscIndex::Iterator RscIndex::Iterator::operator+(const difference_type n) const
    {
        auto temp = *this;
        temp += n;
        return temp;
    }

    RscIndex::Iterator RscIndex::Iterator::operator-(const difference_type n) const
    {
        auto temp = *this;
        temp -= n;
        return temp;
    }

    RscIndex::Iterator operator+(const RscIndex::Iterator::difference_type n, const RscIndex::Iterator& it)
    {
        return it + n;
    }

    RscIndex::Iterator::difference_type RscIndex::Iterator::operator-(const Iterator& other) const
    {
        return static_cast<difference_type>(position_) - static_cast<difference_type>(other.position_);
    }

    RscIndex::Iterator RscIndex::begin() const
    {
        return Iterator{this, 0};
    }

    RscIndex::Iterator RscIndex::end() const
    {
        return Iterator{this, size()};
    }

    std::expected<std::vector<MapRecord>, std::string> RscIndex::loadMapFile(const fs::path& directoryPath)
    {
        auto filePathResult = platform::fs::findFileWithExtension(directoryPath, ".map");
        if (!filePathResult)
            return std::unexpected(filePathResult.error());

        auto reader = platform::fs::BinaryFileReader::open(*filePathResult);
        if (!reader)
            return std::unexpected(reader.error());

        auto header = reader->readStruct<MapHeader>();
        if (!header)
            return std::unexpected(header.error());

        auto records = reader->readStructArray<MapRecord>(header->recordCount);
        if (!records)
            return std::unexpected(records.error());

        return *records;
    }


    std::expected<std::optional<std::vector<IdxRecord>>, std::string> RscIndex::loadIdxFile(const fs::path& directoryPath)
    {
        // index file is optional so no error is returned if it simply isn't found
        auto filePathResult = platform::fs::findFileWithExtension(directoryPath, ".idx");
        if (!filePathResult)
            return std::nullopt;

        auto reader = platform::fs::BinaryFileReader::open(*filePathResult);
        if (!reader)
            return std::unexpected(reader.error());

        auto header = reader->readStruct<IdxHeader>();
        if (!header)
            return std::unexpected(header.error());

        auto records = reader->readStructArray<IdxRecord>(header->length);
        if (!records)
            return std::unexpected(records.error());

        return *records;
    }
}

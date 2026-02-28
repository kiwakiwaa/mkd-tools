//
// kiwakiwaaにより 2026/01/19 に作成されました。
//

#include "MKD/resource/rsc/rsc_index.hpp"
#include "MKD/platform/read_sequence.hpp"

#include <algorithm>
#include <bit>
#include <cassert>
#include <format>
#include <fstream>

namespace MKD
{
    void IdxHeader::swapEndianness() noexcept
    {
        length = std::byteswap(length);
        padding = std::byteswap(padding);
    }


    Result<RscIndex> RscIndex::load(const fs::path& directoryPath)
    {
        auto idxResult = loadIdxFile(directoryPath);
        if (!idxResult)
            return std::unexpected(std::format("Failed to load rsc index at: {}\n{}", directoryPath.string(),
                                               idxResult.error()));

        auto mapResult = loadMapFile(directoryPath);
        if (!mapResult)
            return std::unexpected(std::format("Failed to load rsc map at: {}\n{}", directoryPath.string(),
                                               mapResult.error()));

        auto& [records, mapVersion] = *mapResult;
        return RscIndex(mapVersion, std::move(*idxResult), std::move(records));
    }


    Result<MapRecord> RscIndex::findById(uint32_t itemId) const
    {
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
        const auto it = std::ranges::lower_bound(
            idxRecords,
            itemId,
            std::less{},
            [](const IdxRecord& record) { return record.id(); }
        );

        if (it == idxRecords.end() || it->id() != itemId)
            return std::unexpected(std::format("Item ID {} not found", itemId));

        const size_t mapIdx = it->mapIndex();
        if (mapIdx >= mapRecords_.size())
            return std::unexpected(std::format("Index mismatch for item ID {}", itemId));

        return mapRecords_[mapIdx];
    }


    Result<std::pair<uint32_t, MapRecord>> RscIndex::getByIndex(size_t index) const
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

    uint32_t RscIndex::mapVersion() const
    {
        return mapVersion_;
    }


    size_t RscIndex::size() const noexcept
    {
        return mapRecords_.size();
    }


    bool RscIndex::empty() const noexcept
    {
        return mapRecords_.empty();
    }


    RscIndex::Iterator::Iterator(const RscIndex* index, const size_t pos)
        : index_(index), position_(pos)
    {
    }


    RscIndex::RscIndex(const uint32_t mapVersion, std::optional<std::vector<IdxRecord> >&& idxRecords,
                       std::vector<MapRecord>&& mapRecords)
        : idxRecords_(std::move(idxRecords)), mapRecords_(std::move(mapRecords)), mapVersion_(mapVersion)
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

    Result<std::pair<std::vector<MapRecord>, uint32_t>> RscIndex::loadMapFile(
        const fs::path& directoryPath)
    {
        auto file = findFileWithExtension(directoryPath, ".map")
                .and_then(BinaryFileReader::open);
        if (!file) return std::unexpected(file.error());

        auto seq = file->sequence();
        auto header = seq.read<MapHeader>();
        auto records = seq.readArray<MapRecord>(header.recordCount);

        if (!seq)
            return std::unexpected(seq.error());

        return std::pair{records, header.version};
    }


    Result<std::optional<std::vector<IdxRecord>>> RscIndex::loadIdxFile(
        const fs::path& directoryPath)
    {
        // index file is optional so no error is returned if it simply isn't found
        auto file = findFileWithExtension(directoryPath, ".idx")
                .and_then(BinaryFileReader::open);
        if (!file) return std::nullopt;

        auto seq = file->sequence();
        auto header = seq.read<IdxHeader>();
        auto records = seq.readArray<IdxRecord>(header.length);

        if (!seq)
            return std::unexpected(seq.error());

        return records;
    }
}

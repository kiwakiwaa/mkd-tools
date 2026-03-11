//
// kiwakiwaaにより 2026/01/19 に作成されました。
//

#include "platform/read_sequence.hpp"
#include "resource_store_index.hpp"

#include <algorithm>
#include <bit>
#include <cassert>
#include <format>
#include <fstream>
#include <numeric>

namespace MKD
{
    void ResourceStoreIndexHeader::swapEndianness() noexcept
    {
        length = std::byteswap(length);
        padding = std::byteswap(padding);
    }


    Result<ResourceStoreIndex> ResourceStoreIndex::load(const fs::path& directoryPath)
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
        return ResourceStoreIndex(mapVersion, std::move(*idxResult), std::move(records));
    }


    Result<ResourceStoreMapRecord> ResourceStoreIndex::findById(uint32_t itemId) const
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
            [](const ResourceStoreIndexRecord& record) { return record.id(); }
        );

        if (it == idxRecords.end() || it->id() != itemId)
            return std::unexpected(std::format("Item ID {} not found", itemId));

        const size_t mapIdx = it->mapIndex();
        if (mapIdx >= mapRecords_.size())
            return std::unexpected(std::format("Index mismatch for item ID {}", itemId));

        return mapRecords_[mapIdx];
    }


    Result<std::pair<uint32_t, ResourceStoreMapRecord>> ResourceStoreIndex::getByIndex(size_t index) const
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

    uint32_t ResourceStoreIndex::mapVersion() const
    {
        return mapVersion_;
    }


    size_t ResourceStoreIndex::size() const noexcept
    {
        return mapRecords_.size();
    }


    bool ResourceStoreIndex::empty() const noexcept
    {
        return mapRecords_.empty();
    }


    ResourceStoreIndex::Iterator::Iterator(const ResourceStoreIndex* index, const size_t pos)
        : index_(index), position_(pos)
    {
    }


    ResourceStoreIndex::ResourceStoreIndex(const uint32_t mapVersion, std::optional<std::vector<ResourceStoreIndexRecord> >&& idxRecords,
                       std::vector<ResourceStoreMapRecord>&& mapRecords)
        : idxRecords_(std::move(idxRecords)), mapRecords_(std::move(mapRecords)), mapVersion_(mapVersion)
    {
        buildSortedOrder();
    }

    ResourceStoreIndex::Iterator::value_type ResourceStoreIndex::Iterator::operator*() const
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

    ResourceStoreIndex::Iterator::value_type ResourceStoreIndex::Iterator::operator[](const difference_type n) const
    {
        return *(*this + n);
    }

    ResourceStoreIndex::Iterator& ResourceStoreIndex::Iterator::operator++()
    {
        ++position_;
        return *this;
    }

    ResourceStoreIndex::Iterator ResourceStoreIndex::Iterator::operator++(int)
    {
        const auto temp = *this;
        ++*this;
        return temp;
    }

    ResourceStoreIndex::Iterator& ResourceStoreIndex::Iterator::operator--()
    {
        --position_;
        return *this;
    }

    ResourceStoreIndex::Iterator ResourceStoreIndex::Iterator::operator--(int)
    {
        const auto temp = *this;
        --*this;
        return temp;
    }

    ResourceStoreIndex::Iterator& ResourceStoreIndex::Iterator::operator+=(const difference_type n)
    {
        position_ += n;
        return *this;
    }

    ResourceStoreIndex::Iterator& ResourceStoreIndex::Iterator::operator-=(const difference_type n)
    {
        position_ -= n;
        return *this;
    }

    ResourceStoreIndex::Iterator ResourceStoreIndex::Iterator::operator+(const difference_type n) const
    {
        auto temp = *this;
        temp += n;
        return temp;
    }

    ResourceStoreIndex::Iterator ResourceStoreIndex::Iterator::operator-(const difference_type n) const
    {
        auto temp = *this;
        temp -= n;
        return temp;
    }

    ResourceStoreIndex::Iterator operator+(const ResourceStoreIndex::Iterator::difference_type n, const ResourceStoreIndex::Iterator& it)
    {
        return it + n;
    }

    ResourceStoreIndex::Iterator::difference_type ResourceStoreIndex::Iterator::operator-(const Iterator& other) const
    {
        return static_cast<difference_type>(position_) - static_cast<difference_type>(other.position_);
    }

    ResourceStoreIndex::Iterator ResourceStoreIndex::begin() const
    {
        return Iterator{this, 0};
    }

    ResourceStoreIndex::Iterator ResourceStoreIndex::end() const
    {
        return Iterator{this, size()};
    }

    Result<std::pair<std::vector<ResourceStoreMapRecord>, uint32_t>> ResourceStoreIndex::loadMapFile(
        const fs::path& directoryPath)
    {
        auto file = findFileWithExtension(directoryPath, ".map")
                .and_then(BinaryFileReader::open);
        if (!file) return std::unexpected(file.error());

        auto seq = file->sequence();
        auto header = seq.read<ResourceStoreMapHeader>();
        auto records = seq.readArray<ResourceStoreMapRecord>(header.recordCount);

        if (!seq)
            return std::unexpected(seq.error());

        return std::pair{records, header.version};
    }


    Result<std::optional<std::vector<ResourceStoreIndexRecord>>> ResourceStoreIndex::loadIdxFile(
        const fs::path& directoryPath)
    {
        // index file is optional so no error is returned if it simply isn't found
        auto file = findFileWithExtension(directoryPath, ".idx")
                .and_then(BinaryFileReader::open);
        if (!file) return std::nullopt;

        auto seq = file->sequence();
        auto header = seq.read<ResourceStoreIndexHeader>();
        auto records = seq.readArray<ResourceStoreIndexRecord>(header.length);

        if (!seq)
            return std::unexpected(seq.error());

        return records;
    }


    void ResourceStoreIndex::buildSortedOrder()
    {
        sortedOrder_.resize(mapRecords_.size());
        std::iota(sortedOrder_.begin(), sortedOrder_.end(), size_t{0});
        std::ranges::sort(sortedOrder_, [this](const size_t a, const size_t b) {
            const auto& ra = mapRecords_[a];
            const auto& rb = mapRecords_[b];
            if (ra.chunkGlobalOffset != rb.chunkGlobalOffset)
                return ra.chunkGlobalOffset < rb.chunkGlobalOffset;
            return ra.chunkGlobalOffset < rb.chunkGlobalOffset;
        });
    }
}

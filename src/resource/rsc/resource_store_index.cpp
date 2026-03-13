//
// kiwakiwaaにより 2026/01/19 に作成されました。
//

#include "platform/mmap_file.hpp"
#include "resource_store_index.hpp"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <format>
#include <numeric>

namespace MKD
{
    namespace
    {
        template<typename T>
        const T* ptrAt(const MappedFile& file, const size_t offset)
        {
            if (offset + sizeof(T) > file.size())
                return nullptr;
            return reinterpret_cast<const T*>(file.data().data() + offset);
        }

        Result<MappedFile> openSingleFileWithExtension(const fs::path& directoryPath, std::string_view extension)
        {
            if (!fs::exists(directoryPath))
                return std::unexpected(std::format("Directory not found: {}", directoryPath.string()));

            if (!fs::is_directory(directoryPath))
                return std::unexpected(std::format("Not a directory: {}", directoryPath.string()));

            std::optional<fs::path> match;
            for (const auto& entry : fs::directory_iterator(directoryPath))
            {
                if (!entry.is_regular_file() || entry.path().extension() != extension)
                    continue;

                if (match)
                    return std::unexpected(std::format(
                        "Multiple {} files found in: {}",
                        extension,
                        directoryPath.string()));

                match = entry.path();
            }

            if (!match)
                return std::unexpected(std::format("No {} file found in: {}", extension, directoryPath.string()));

            return MappedFile::open(*match);
        }
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


    ResourceStoreIndex::ResourceStoreIndex(const uint32_t mapVersion, std::optional<std::vector<ResourceStoreIndexRecord>>&& idxRecords,
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
        auto file = openSingleFileWithExtension(directoryPath, ".map");
        if (!file) return std::unexpected(file.error());

        const auto* header = ptrAt<ResourceStoreMapHeader>(*file, 0);
        if (!header)
            return std::unexpected("Map file too small for header");

        constexpr size_t recordsOffset = sizeof(ResourceStoreMapHeader);
        const size_t recordsBytes = static_cast<size_t>(header->recordCount) * sizeof(ResourceStoreMapRecord);
        if (recordsOffset + recordsBytes > file->size())
            return std::unexpected("Map records exceed file size");

        std::vector<ResourceStoreMapRecord> records(header->recordCount);
        std::memcpy(records.data(), file->data().data() + recordsOffset, recordsBytes);

        return std::make_pair(std::move(records), header->version);
    }


    Result<std::optional<std::vector<ResourceStoreIndexRecord>>> ResourceStoreIndex::loadIdxFile(const fs::path& directoryPath)
    {
        if (!fs::exists(directoryPath))
            return std::unexpected(std::format("Directory not found: {}", directoryPath.string()));

        if (!fs::is_directory(directoryPath))
            return std::unexpected(std::format("Not a directory: {}", directoryPath.string()));

        std::optional<fs::path> idxPath;
        for (const auto& entry : fs::directory_iterator(directoryPath))
        {
            if (!entry.is_regular_file() || entry.path().extension() != ".idx")
                continue;

            if (idxPath)
                return std::unexpected(std::format("Multiple .idx files found in: {}", directoryPath.string()));

            idxPath = entry.path();
        }

        if (!idxPath)
            return std::optional<std::vector<ResourceStoreIndexRecord>>{};

        auto file = MappedFile::open(*idxPath);
        if (!file) return std::unexpected(file.error());

        const auto* header = ptrAt<ResourceStoreIndexHeader>(*file, 0);
        if (!header)
            return std::unexpected("Idx file too small for header");

        const size_t recordsOffset = sizeof(ResourceStoreIndexHeader);
        const size_t recordsBytes = static_cast<size_t>(header->length) * sizeof(ResourceStoreIndexRecord);
        if (recordsOffset + recordsBytes > file->size())
            return std::unexpected("Idx records exceed file size");

        std::vector<ResourceStoreIndexRecord> records(header->length);
        std::memcpy(records.data(), file->data().data() + recordsOffset, recordsBytes);

        return std::optional{std::move(records)};
    }


    void ResourceStoreIndex::buildSortedOrder()
    {
        if (!idxRecords_.has_value())
        {
            sortedOrder_.resize(mapRecords_.size());
            std::iota(sortedOrder_.begin(), sortedOrder_.end(), 0);
            return;
        }

        sortedOrder_.resize(idxRecords_->size());
        std::iota(sortedOrder_.begin(), sortedOrder_.end(), 0);

        std::ranges::sort(sortedOrder_, {}, [this](size_t i)
        {
            return (*idxRecords_)[i].id();
        });
    }
}

//
// kiwakiwaaにより 2026/01/16 に作成されました。
//

#include "platform/mmap_file.hpp"
#include "named_resource_store_index.hpp"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <format>

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


    Result<NamedResourceStoreIndex> NamedResourceStoreIndex::load(const fs::path& directoryPath)
    {
        auto file = openSingleFileWithExtension(directoryPath, ".nidx");
        if (!file) return std::unexpected(file.error());

        const auto* header = ptrAt<NamedResourceStoreIndexHeader>(*file, 0);
        if (!header)
            return std::unexpected("Index file too small for header");

        constexpr size_t recordsOffset = HEADER_SIZE;
        const size_t recordsBytes = static_cast<size_t>(header->recordCount) * sizeof(NamedResourceStoreIndexRecord);
        if (recordsOffset + recordsBytes > file->size())
            return std::unexpected("Index records exceed file size");

        std::vector<NamedResourceStoreIndexRecord> records(header->recordCount);
        std::memcpy(records.data(), file->data().data() + recordsOffset, recordsBytes);

        const size_t stringsOffset = recordsOffset + recordsBytes;
        std::string idStrings;
        idStrings.resize(file->size() - stringsOffset);
        std::memcpy(idStrings.data(), file->data().data() + stringsOffset, idStrings.size());

        return NamedResourceStoreIndex(
            std::move(records),
            std::move(idStrings),
            stringsOffset
        );
    }


    Result<NamedResourceStoreIndexRecord> NamedResourceStoreIndex::findById(std::string_view id) const
    {
        const auto it = std::ranges::lower_bound(records_, id,
                                                 [](std::string_view a, std::string_view b) {
                                                     return a < b;
                                                 },
                                                 [this](const NamedResourceStoreIndexRecord& record) -> std::string_view {
                                                     const auto result = this->getIdAt(record.idOffset());
                                                     return result ? *result : std::string_view{};
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


    Result<std::pair<std::string_view, NamedResourceStoreIndexRecord>> NamedResourceStoreIndex::getByIndex(
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


    size_t NamedResourceStoreIndex::size() const noexcept
    {
        return records_.size();
    }


    bool NamedResourceStoreIndex::empty() const noexcept
    {
        return size() == 0;
    }


    NamedResourceStoreIndex::Iterator::Iterator(const NamedResourceStoreIndex* index, const size_t pos)
        : index_(index), position_(pos)
    {
    }

    NamedResourceStoreIndex::Iterator::value_type NamedResourceStoreIndex::Iterator::operator*() const
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

    NamedResourceStoreIndex::Iterator::value_type NamedResourceStoreIndex::Iterator::operator[](difference_type n) const
    {
        return *(*this + n);
    }


    NamedResourceStoreIndex::Iterator& NamedResourceStoreIndex::Iterator::operator++()
    {
        ++position_;
        return *this;
    }

    NamedResourceStoreIndex::Iterator NamedResourceStoreIndex::Iterator::operator++(int)
    {
        const auto temp = *this;
        ++*this;
        return temp;
    }

    NamedResourceStoreIndex::Iterator& NamedResourceStoreIndex::Iterator::operator--()
    {
        --position_;
        return *this;
    }

    NamedResourceStoreIndex::Iterator NamedResourceStoreIndex::Iterator::operator--(int)
    {
        const auto temp = *this;
        --*this;
        return temp;
    }

    // Arithmetic
    NamedResourceStoreIndex::Iterator& NamedResourceStoreIndex::Iterator::operator+=(const difference_type n)
    {
        position_ += n;
        return *this;
    }

    NamedResourceStoreIndex::Iterator& NamedResourceStoreIndex::Iterator::operator-=(const difference_type n)
    {
        position_ -= n;
        return *this;
    }

    NamedResourceStoreIndex::Iterator NamedResourceStoreIndex::Iterator::operator+(const difference_type n) const
    {
        auto temp = *this;
        temp += n;
        return temp;
    }

    NamedResourceStoreIndex::Iterator NamedResourceStoreIndex::Iterator::operator-(const difference_type n) const
    {
        auto temp = *this;
        temp -= n;
        return temp;
    }

    NamedResourceStoreIndex::Iterator operator+(const NamedResourceStoreIndex::Iterator::difference_type n, const NamedResourceStoreIndex::Iterator& it)
    {
        return it + n;
    }

    NamedResourceStoreIndex::Iterator::difference_type NamedResourceStoreIndex::Iterator::operator-(const Iterator& other) const
    {
        return static_cast<difference_type>(position_) - static_cast<difference_type>(other.position_);
    }

    NamedResourceStoreIndex::Iterator NamedResourceStoreIndex::begin() const
    {
        return Iterator{this, 0};
    }

    NamedResourceStoreIndex::Iterator NamedResourceStoreIndex::end() const
    {
        return Iterator{this, size()};
    }


    NamedResourceStoreIndex::NamedResourceStoreIndex(std::vector<NamedResourceStoreIndexRecord>&& records, std::string&& idStrings, const size_t headerSize)
        : records_(std::move(records))
          , idStrings_(std::move(idStrings))
          , headerSize_(headerSize)
    {
    }


    Result<std::string_view> NamedResourceStoreIndex::getIdAt(size_t offset) const
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
}

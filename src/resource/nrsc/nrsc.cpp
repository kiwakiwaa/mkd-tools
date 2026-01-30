//
// kiwakiwaaにより 2026/01/16 に作成されました。
//

#include "monokakido/resource/nrsc/nrsc.hpp"

#include <cassert>
#include <format>

namespace monokakido::resource
{
    std::expected<Nrsc, std::string> Nrsc::open(const fs::path& directoryPath)
    {
        auto indexResult = NrscIndex::load(directoryPath);
        if (!indexResult)
            return std::unexpected(std::format("Failed to load index: {}", indexResult.error()));

        auto dataResult = NrscData::load(directoryPath);
        if (!dataResult)
            return std::unexpected(std::format("Failed to load data: {}", dataResult.error()));

        return Nrsc{std::move(*indexResult), std::move(*dataResult)};
    }


    std::expected<std::span<const uint8_t>, std::string> Nrsc::get(std::string_view id) const
    {
        auto record = index_.findById(id);
        if (!record)
            return std::unexpected(record.error());

        return data_.get(*record);
    }


    std::expected<ResourceItem, std::string> Nrsc::getByIndex(const size_t index) const
    {
        auto result = index_.getByIndex(index);
        if (!result)
            return std::unexpected(result.error());

        auto [id, record] = *result;
        auto dataResult = data_.get(record);
        if (!dataResult)
            return std::unexpected(dataResult.error());

        return ResourceItem{id, *dataResult};
    }


    size_t Nrsc::size() const noexcept
    {
        return index_.size();
    }


    bool Nrsc::empty() const noexcept
    {
        return index_.empty();
    }


    Nrsc::Iterator::Iterator(const Nrsc* nrsc, const size_t index)
        : nrsc_(nrsc), index_(index)
    {
    }

    Nrsc::Iterator::value_type Nrsc::Iterator::operator*() const
    {
        assert(nrsc_ != nullptr && "Dereferencing invalid iterator");
        assert(index_ < nrsc_->size() && "Dereferencing end iterator");

        auto result = nrsc_->getByIndex(index_);
        if (!result)
        {
            throw std::runtime_error(
                std::format("Nrsc iteration failed at position {}: {}",
                    index_, result.error()));
        }

        return *result;
    }

    Nrsc::Iterator& Nrsc::Iterator::operator++()
    {
        ++index_;
        return *this;
    }

    Nrsc::Iterator Nrsc::Iterator::operator++(int)
    {
        const Iterator tmp = *this;
        ++*this;
        return tmp;
    }

    bool Nrsc::Iterator::operator==(const Iterator& other) const
    {
        return nrsc_ == other.nrsc_ && index_ == other.index_ ;
    }

    bool Nrsc::Iterator::operator!=(const Iterator& other) const
    {
        return !(*this == other);
    }


    Nrsc::Nrsc(NrscIndex&& index, NrscData&& data)
        : index_(std::move(index)), data_(std::move(data))
    {
    }


    Nrsc::Iterator Nrsc::begin() const
    {
        return Iterator{this, 0};
    }


    Nrsc::Iterator Nrsc::end() const
    {
        return Iterator{this, size()};
    }
}

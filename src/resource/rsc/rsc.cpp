//
// kiwakiwaaにより 2026/02/01 に作成されました。
//

#include "monokakido/resource/rsc/rsc.hpp"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <ranges>

namespace monokakido
{
    std::expected<Rsc, std::string> Rsc::open(const fs::path& directoryPath)
    {
        auto indexResult = RscIndex::load(directoryPath);
        if (!indexResult)
            return std::unexpected(std::format("Failed to load rsc index: {}", indexResult.error()));

        auto dictId = getDictId(directoryPath);
        if (!dictId)
            return std::unexpected(std::format("Failed to get dict id from path: {}", directoryPath.string()));

        auto dataResult = RscData::load(directoryPath, *dictId);
        if (!dataResult)
            return std::unexpected(std::format("Failed to load rsc data: {}", dataResult.error()));

        return Rsc(std::move(*indexResult), std::move(*dataResult));
    }


    std::expected<std::span<const uint8_t>, std::string> Rsc::get(const uint32_t itemId) const
    {
        const auto record = index_.findById(itemId);
        if (!record)
            return std::unexpected(std::format("Failed to get record from rsc index: {}", itemId));

        return data_.get(*record);
    }


    std::expected<RscItem, std::string> Rsc::getByIndex(const size_t index) const
    {
        auto result = index_.getByIndex(index);
        if (!result)
            return std::unexpected(result.error());

        auto [id, record] = *result;
        auto dataResult = data_.get(record);
        if (!dataResult)
            return std::unexpected(dataResult.error());

        return RscItem{id, *dataResult};
    }


    size_t Rsc::size() const noexcept
    {
        return index_.size();
    }


    bool Rsc::empty() const noexcept
    {
        return index_.empty();
    }


    Rsc::Iterator::Iterator(const Rsc* rsc, const size_t index)
        : rsc_(rsc), index_(index)
    {
    }

    Rsc::Iterator::value_type Rsc::Iterator::operator*() const
    {
        assert(rsc_ != nullptr && "Dereferencing invalid iterator");
        assert(index_ < rsc_->size() && "Dereferencing end iterator");

        auto result = rsc_->getByIndex(index_);
        if (!result)
        {
            throw std::runtime_error(
                std::format("Rsc iteration failed at position {}: {}",
                    index_, result.error()));
        }

        return *result;
    }

    Rsc::Iterator& Rsc::Iterator::operator++()
    {
        ++index_;
        return *this;
    }

    Rsc::Iterator Rsc::Iterator::operator++(int)
    {
        const Iterator tmp = *this;
        ++*this;
        return tmp;
    }

    bool Rsc::Iterator::operator==(const Iterator& other) const
    {
        return rsc_ == other.rsc_ && index_ == other.index_;
    }

    bool Rsc::Iterator::operator!=(const Iterator& other) const
    {
        return !(*this == other);
    }

    Rsc::Iterator Rsc::begin() const
    {
        return Iterator{this, 0};
    }

    Rsc::Iterator Rsc::end() const
    {
        return Iterator{this, size()};
    }


    Rsc::Rsc(RscIndex&& index, RscData&& data)
    : index_(std::move(index)), data_(std::move(data))
    {
    }


    std::optional<std::string> Rsc::getDictId(const fs::path& directoryPath)
    {
        std::vector<std::string> components;
        for (const auto& part : directoryPath)
            components.emplace_back(part.string());

        for (size_t i = 0; i < components.size(); ++i)
        {
            if (components[i] == "dictionaries")
                return components.at(i + 1);
        }

        return std::nullopt;
    }
}

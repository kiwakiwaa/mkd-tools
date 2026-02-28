//
// kiwakiwaaにより 2026/01/30 に作成されました。
//

#pragma once

#include "MKD/resource/rsc/rsc_index.hpp"
#include "MKD/resource/rsc/rsc_data.hpp"

#include <expected>
#include <iterator>
#include <optional>
#include <string>

namespace fs = std::filesystem;

namespace MKD
{
    struct RscItem
    {
        uint32_t itemId;
        std::span<const uint8_t> data;
    };

    class Rsc
    {
    public:
        /**
         * Factory method to open .rsc dictionary contents from a dictionary
         *
         * @param directoryPath Directory path containing the .idx/.map & .rsc files
         * @return
         */
        static Result<Rsc> open(const fs::path& directoryPath, std::string_view dictId = "");

        /**
         * This will basically look up the itemId first with RscIndex, if it finds a
         * map record, it will use that to retrieve the data from the .rsc
         * this works well for dictionary entries where you want to get the xml data for a given itemId
         */
        [[nodiscard]] Result<OwnedSpan> get(uint32_t itemId) const;

        /**
         * Get RscItem by index
         * @param index index
         * @return Rscitem or error string if failure
         */
        [[nodiscard]] Result<RscItem> getByIndex(size_t index) const;

        /**
         * Get total number of records
         * @return Number of records
         */
        [[nodiscard]] size_t size() const noexcept;

        /**
         * @return true if there are no records
         */
        [[nodiscard]] bool empty() const noexcept;


        class Iterator
        {
        public:
            using iterator_category = std::forward_iterator_tag;
            using iterator_concept = std::forward_iterator_tag;
            using difference_type = std::ptrdiff_t;
            using value_type = RscItem;
            using pointer = value_type*;
            using reference = value_type;

            Iterator() noexcept = default;
            Iterator(const Rsc* rsc, size_t index);

            value_type operator*() const;

            Iterator& operator++();
            Iterator operator++(int);

            bool operator==(const Iterator& other) const;
            bool operator!=(const Iterator& other) const;

        private:
            const Rsc* rsc_ = nullptr;
            size_t index_ = 0;
        };

        [[nodiscard]] Iterator begin() const;
        [[nodiscard]] Iterator end() const;

        static_assert(std::forward_iterator<Iterator>);

    private:
        explicit Rsc(RscIndex&& index, RscData&& data);

        RscIndex index_;
        RscData data_;
    };


}
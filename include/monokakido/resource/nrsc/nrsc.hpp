//
// kiwakiwaaにより 2026/01/16 に作成されました。
//

#pragma once

#include "monokakido/resource/common.hpp"
#include "nrsc_index.hpp"
#include "nrsc_data.hpp"

#include <expected>
#include <filesystem>
#include <iterator>
#include <string>


namespace fs = std::filesystem;

namespace monokakido::resource
{
    class NrscData;


    class Nrsc
    {
    public:
        /**
         * Factory method to open a .nrsc resource from a directory
         * @param directoryPath Directory path containing the resources
         * @return Nrsc resource class or error string if failure
         */
        static std::expected<Nrsc, std::string> open(const fs::path& directoryPath);


        /**
         * Get resource by string ID
         * - Calls findById from NrscIndex to do a binary search in the index
         * - O(log(n))
         * @param id String ID of the resource
         * @return Data view of resource data, error string if failure
         */
        [[nodiscard]] std::expected<std::span<const uint8_t>, std::string> get(std::string_view id) const;


        /**
         * Get resource by index
         * - Calls getByIndex from NrscIndex
         * - O(1)
         * @param index
         * @return
         */
        [[nodiscard]] std::expected<ResourceItem, std::string> getByIndex(size_t index) const;


        /**
         * Get total number of resources
         * @return Number of resources
         */
        [[nodiscard]] size_t size() const noexcept;


        /**
         * @return true if there are no resources
         */
        [[nodiscard]] bool empty() const noexcept;


        class Iterator
        {
        public:
            using iterator_category = std::forward_iterator_tag;
            using iterator_concept = std::forward_iterator_tag;
            using difference_type = std::ptrdiff_t;
            using value_type = ResourceItem;
            using pointer = value_type*;
            using reference = value_type;

            Iterator() noexcept = default;
            Iterator(const Nrsc* nrsc, size_t index);

            value_type operator*() const;

            Iterator& operator++();
            Iterator operator++(int);

            bool operator==(const Iterator& other) const;
            bool operator!=(const Iterator& other) const;


        private:
            const Nrsc* nrsc_ = nullptr;
            size_t index_ = 0;

        };

        Iterator begin() const;
        Iterator end() const;

        static_assert(std::forward_iterator<Iterator>);

    private:

        explicit Nrsc(NrscIndex&& index, NrscData&& data);

        NrscIndex index_;
        NrscData data_;

    };

}
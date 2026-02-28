//
// kiwakiwaaにより 2026/02/28 に作成されました。
//

#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <utility>
#include <vector>

namespace MKD
{
    /*
     * A span of bytes that keeps its backing memory alive
     */
    class OwnedSpan
    {
    public:
        // non owning view
        explicit OwnedSpan(const std::span<const uint8_t> span)
            : span_(span)
        {
        }

        // Owning view into a shared buffer.
        OwnedSpan(std::shared_ptr<const std::vector<uint8_t>> owner, const std::span<const uint8_t> span)
            : owner_(std::move(owner)), span_(span)
        {
        }

        // Owning view of a shared buffer
        explicit OwnedSpan(std::shared_ptr<const std::vector<uint8_t>> owner)
            : owner_(std::move(owner))
            , span_(owner_->data(), owner_->size())
        {
        }

        [[nodiscard]] const uint8_t* data() const noexcept { return span_.data(); }
        [[nodiscard]] size_t size() const noexcept { return span_.size(); }
        [[nodiscard]] bool empty() const noexcept { return span_.empty(); }

        [[nodiscard]] std::span<const uint8_t> span() const noexcept { return span_; }
        explicit operator std::span<const uint8_t>() const noexcept { return span_; }

        [[nodiscard]] auto begin() const noexcept { return span_.begin(); }
        [[nodiscard]] auto end() const noexcept { return span_.end(); }

        [[nodiscard]] uint8_t operator[](const size_t i) const { return span_[i]; }

    private:
        std::shared_ptr<const std::vector<uint8_t>> owner_;
        std::span<const uint8_t> span_;
    };
}
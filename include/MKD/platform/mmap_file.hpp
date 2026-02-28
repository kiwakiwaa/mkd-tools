//
// kiwakiwaaにより 2026/02/28 に作成されました。
//

#pragma once

#include "MKD/result.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>

namespace MKD
{
    /// RAII read-only memory-mapped file.
    class MappedFile
    {
    public:
        static Result<MappedFile> open(const std::filesystem::path& path);

        ~MappedFile();

        MappedFile(const MappedFile&) = delete;
        MappedFile& operator=(const MappedFile&) = delete;

        MappedFile(MappedFile&& other) noexcept;
        MappedFile& operator=(MappedFile&& other) noexcept;

        [[nodiscard]] std::span<const uint8_t> data() const noexcept;
        [[nodiscard]] size_t size() const noexcept;

        /// Return a subspan, or error if out of bounds.
        [[nodiscard]] Result<std::span<const uint8_t>> slice(size_t offset, size_t length) const;

    private:
        MappedFile(void* base, size_t size);
        void release() noexcept;

        void* base_ = nullptr;
        size_t size_ = 0;
    };
}
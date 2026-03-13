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
    /*
     * Read only mmap file
     */
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

        /**
         * Return a subspan of the memory mapped data
         * @param offset starting offset
         * @param length length of the data
         * @return Subspan of the data or error string if failure
         */
        [[nodiscard]] Result<std::span<const uint8_t>> slice(size_t offset, size_t length) const;

    private:
#if defined(_WIN32)
        MappedFile(void* base, void* mappingHandle, size_t size);
#else
        MappedFile(void* base, size_t size);
#endif
        void release() noexcept;

        void* base_ = nullptr;
#if defined(_WIN32)
        void* mappingHandle_ = nullptr;
#endif
        size_t size_ = 0;
    };
}

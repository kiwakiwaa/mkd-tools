//
// kiwakiwaaにより 2026/02/28 に作成されました。
//

#include "mmap_file.hpp"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstring>
#include <format>

namespace MKD
{
    Result<MappedFile> MappedFile::open(const std::filesystem::path& path)
    {
        const int fd = ::open(path.c_str(), O_RDONLY);
        if (fd < 0)
            return std::unexpected(std::format("Failed to open '{}': {}",
                path.string(), std::strerror(errno)));

        struct stat st{};
        if (::fstat(fd, &st) != 0)
        {
            ::close(fd);
            return std::unexpected(std::format("Failed to stat '{}': {}",
                path.string(), std::strerror(errno)));
        }

        const auto size = static_cast<size_t>(st.st_size);
        void* base = ::mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);
        ::close(fd);

        if (base == MAP_FAILED)
            return std::unexpected(std::format("Failed to mmap '{}': {}",
                path.string(), std::strerror(errno)));

        return MappedFile{base, size};
    }


    MappedFile::MappedFile(void* base, const size_t size)
        : base_(base), size_(size)
    {
    }


    MappedFile::~MappedFile() { release(); }


    MappedFile::MappedFile(MappedFile&& other) noexcept
        : base_(other.base_), size_(other.size_)
    {
        other.base_ = nullptr;
        other.size_ = 0;
    }


    MappedFile& MappedFile::operator=(MappedFile&& other) noexcept
    {
        if (this != &other)
        {
            release();
            base_ = other.base_;
            size_ = other.size_;
            other.base_ = nullptr;
            other.size_ = 0;
        }
        return *this;
    }


    std::span<const uint8_t> MappedFile::data() const noexcept
    {
        return {static_cast<const uint8_t*>(base_), size_};
    }


    size_t MappedFile::size() const noexcept { return size_; }


    Result<std::span<const uint8_t>> MappedFile::slice(size_t offset, size_t length) const
    {
        if (offset + length > size_)
            return std::unexpected(std::format(
                "Slice [{}, +{}) exceeds file size {}", offset, length, size_));

        return data().subspan(offset, length);
    }


    void MappedFile::release() noexcept
    {
        if (base_)
        {
            ::munmap(base_, size_);
            base_ = nullptr;
            size_ = 0;
        }
    }
}
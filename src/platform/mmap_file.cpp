//
// kiwakiwaaにより 2026/02/28 に作成されました。
//

#include "mmap_file.hpp"

#include <format>

#if defined(_WIN32)
#include <windows.h>

namespace
{
    std::string lastErrorMessage(const DWORD error)
    {
        LPSTR buffer = nullptr;
        const DWORD length = ::FormatMessageA(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr,
            error,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            reinterpret_cast<LPSTR>(&buffer),
            0,
            nullptr);

        if (length == 0 || buffer == nullptr)
            return std::format("Windows error {}", error);

        std::string message(buffer, length);
        ::LocalFree(buffer);

        while (!message.empty() &&
               (message.back() == '\r' || message.back() == '\n' || message.back() == ' ' || message.back() == '.'))
            message.pop_back();

        return message;
    }
}
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#endif

namespace MKD
{
    Result<MappedFile> MappedFile::open(const std::filesystem::path& path)
    {
#if defined(_WIN32)
        const std::wstring widePath = path.wstring();

        const HANDLE file = ::CreateFileW(
            widePath.c_str(),
            GENERIC_READ,
            FILE_SHARE_READ,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);

        if (file == INVALID_HANDLE_VALUE)
            return std::unexpected(std::format(
                "Failed to open '{}': {}",
                path.string(), lastErrorMessage(::GetLastError())));

        LARGE_INTEGER fileSize{};
        if (!::GetFileSizeEx(file, &fileSize))
        {
            const auto message = std::format(
                "Failed to stat '{}': {}",
                path.string(), lastErrorMessage(::GetLastError()));
            ::CloseHandle(file);
            return std::unexpected(message);
        }

        if (fileSize.QuadPart < 0)
        {
            ::CloseHandle(file);
            return std::unexpected(std::format(
                "Invalid file size for '{}': {}",
                path.string(), fileSize.QuadPart));
        }

        const auto size = static_cast<size_t>(fileSize.QuadPart);
        if (size == 0)
        {
            ::CloseHandle(file);
            return MappedFile{nullptr, nullptr, 0};
        }

        const HANDLE mapping = ::CreateFileMappingW(file, nullptr, PAGE_READONLY, 0, 0, nullptr);
        if (mapping == nullptr)
        {
            const auto message = std::format(
                "Failed to create file mapping for '{}': {}",
                path.string(), lastErrorMessage(::GetLastError()));
            ::CloseHandle(file);
            return std::unexpected(message);
        }

        void* base = ::MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0);
        if (base == nullptr)
        {
            const auto message = std::format(
                "Failed to map view of '{}': {}",
                path.string(), lastErrorMessage(::GetLastError()));
            ::CloseHandle(mapping);
            ::CloseHandle(file);
            return std::unexpected(message);
        }

        return MappedFile{base, mapping, size};
#else
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
        if (size == 0)
        {
            ::close(fd);
            return MappedFile{nullptr, 0};
        }

        void* base = ::mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);
        ::close(fd);

        if (base == MAP_FAILED)
            return std::unexpected(std::format("Failed to mmap '{}': {}",
                                               path.string(), std::strerror(errno)));

        return MappedFile{base, size};
#endif
    }

#if defined(_WIN32)
    MappedFile::MappedFile(void* base, void* mappingHandle, const size_t size)
        : base_(base), mappingHandle_(mappingHandle), size_(size)
    {
    }
#else
    MappedFile::MappedFile(void* base, const size_t size)
        : base_(base), size_(size)
    {
    }
#endif


    MappedFile::~MappedFile() { release(); }


    MappedFile::MappedFile(MappedFile&& other) noexcept
#if defined(_WIN32)
    : base_ (other.base_), mappingHandle_ (other.mappingHandle_), size_ (other.size_)
#else
        : base_(other.base_), size_(other.size_)
#endif
    {
        other.base_ = nullptr;
#if defined(_WIN32)
        other.mappingHandle_ = nullptr;
#endif
        other.size_ = 0;
    }


    MappedFile& MappedFile::operator=(MappedFile&& other) noexcept
    {
        if (this != &other)
        {
            release();
            base_ = other.base_;
#if defined(_WIN32)
            mappingHandle_ = other.mappingHandle_;
#endif
            size_ = other.size_;
            other.base_ = nullptr;
#if defined(_WIN32)
            other.mappingHandle_ = nullptr;
#endif
            other.size_ = 0;
        }
        return *this;
    }


    std::span<const uint8_t> MappedFile::data() const noexcept
    {
        return {static_cast<const uint8_t*>(base_), size_};
    }


    size_t MappedFile::size() const noexcept
    {
        return size_;
    }


    Result<std::span<const uint8_t>> MappedFile::slice(size_t offset, size_t length) const
    {
        if (offset + length > size_)
            return std::unexpected(std::format(
                "Slice [{}, +{}) exceeds file size {}", offset, length, size_));

        return data().subspan(offset, length);
    }


    void MappedFile::release() noexcept
    {
#if defined(_WIN32)
        if (base_)
        {
            ::UnmapViewOfFile(base_);
            base_ = nullptr;
        }

        if (mappingHandle_)
        {
            ::CloseHandle(static_cast<HANDLE>(mappingHandle_));
            mappingHandle_ = nullptr;
        }
#else
        if (base_)
        {
            ::munmap(base_, size_);
            base_ = nullptr;
        }
#endif
        size_ = 0;
    }
}

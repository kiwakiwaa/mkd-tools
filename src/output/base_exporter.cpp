//
// kiwakiwaaにより 2026/02/01 に作成されました。
//

#include "MKD/output/base_exporter.hpp"

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/fcntl.h>
#include <unistd.h>
#endif

#include <cstring>
#include <format>
#include <fstream>


namespace MKD
{
    Result<void> BaseExporter::writeData(const std::span<const uint8_t> data, const fs::path& path)
    {
#ifdef _WIN32
        HANDLE hFile = ::CreateFileW(
            path.c_str(),
            GENERIC_WRITE,
            0,
            nullptr,
            CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            nullptr
        );
        if (hFile == INVALID_HANDLE_VALUE)
            return std::unexpected(std::format("open failed: error code {}", ::GetLastError()));

        const uint8_t* ptr = data.data();
        size_t remaining = data.size();

        while (remaining > 0)
        {
            DWORD written = 0;
            const DWORD toWrite = static_cast<DWORD>(std::min(remaining, static_cast<size_t>(MAXDWORD)));
            if (!::WriteFile(hFile, ptr, toWrite, &written, nullptr))
            {
                const DWORD err = ::GetLastError();
                ::CloseHandle(hFile);
                return std::unexpected(std::format("write failed: error code {}", err));
            }
            ptr += written;
            remaining -= written;
        }

        ::CloseHandle(hFile);
#else
        const int fd = ::open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0)
            return std::unexpected(std::format("open failed: {}", strerror(errno)));

        const uint8_t* ptr = data.data();
        size_t remaining = data.size();

        while (remaining > 0)
        {
            const ssize_t written = ::write(fd, ptr, remaining);
            if (written < 0)
            {
                const int err = errno;
                ::close(fd);
                return std::unexpected(std::format("write failed: {}", strerror(err)));
            }
            ptr += written;
            remaining -= written;
        }

        ::close(fd);
#endif
        return {};
    }


    bool BaseExporter::shouldSkipExisting(const fs::path& path, const bool overwrite)
    {
        return !overwrite && fs::exists(path);
    }
}

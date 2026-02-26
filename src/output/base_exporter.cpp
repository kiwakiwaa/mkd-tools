//
// kiwakiwaaにより 2026/02/01 に作成されました。
//

#include "MKD/output/base_exporter.hpp"

#include <sys/fcntl.h>
#include <unistd.h>

#include <format>
#include <fstream>


namespace MKD
{
    std::expected<void, std::string> BaseExporter::writeData(const std::span<const uint8_t> data, const fs::path& path)
    {
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
        return {};
    }


    bool BaseExporter::shouldSkipExisting(const fs::path& path, const bool overwrite)
    {
        return !overwrite && fs::exists(path);
    }
}

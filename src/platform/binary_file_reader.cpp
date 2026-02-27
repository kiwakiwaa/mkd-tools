#include "MKD/platform/binary_file_reader.hpp"

#include <fstream>
#include <sstream>

namespace MKD {
    std::string makeFilestreamError(const std::ifstream& file, std::string_view context)
    {
        std::string error = std::format("Failed to {}: ", context);

        if (file.eof())
        {
            error += "unexpected end of file";
        }
        else if (file.bad())
        {
            const std::error_code ec(errno, std::generic_category());
            error += std::format("system error - {}", ec.message());
        }
        else if (file.fail())
        {
            error += "format or I/O error";
        }
        else
        {
            error += "unknown error";
        }

        return error;
    }

    std::expected<BinaryFileReader, std::string> BinaryFileReader::open(const std::filesystem::path& filePath)
    {
        std::ifstream file(filePath);
        if (!file)
            return std::unexpected(std::format("Failed to open file '{}'", filePath.string()));

        return BinaryFileReader(std::move(file));
    }


    std::expected<void, std::string> BinaryFileReader::seek(const size_t offset) const
    {
        file_.seekg(static_cast<std::streamoff>(offset));
        if (!file_)
        {
            return std::unexpected(std::format("Failed to seek to offset '{}'",
                                               offset));
        }
        return {};
    }


    std::expected<void, std::string> BinaryFileReader::readBytes(std::span<uint8_t> buffer) const
    {
        file_.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(buffer.size()));

        if (!file_)
            return std::unexpected(makeFilestreamError(file_, std::format("read {} bytes", buffer.size())));

        return {};
    }


    std::expected<std::vector<uint8_t>, std::string> BinaryFileReader::readBytes(const size_t count) const
    {
        std::vector<uint8_t> data(count);
        file_.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(count));

        if (!file_)
            return std::unexpected(makeFilestreamError(file_, std::format("read {} bytes", count)));

        return data;
    }


    std::expected<std::string, std::string> BinaryFileReader::readBytesIntoString(const size_t size) const
    {
        std::string data;
        data.resize(size);

        const auto bytesToRead = static_cast<std::streamsize>(size);
        file_.read(data.data(), bytesToRead);

        if (!file_)
            return std::unexpected(makeFilestreamError(file_, "read bytes into string"));

        return data;
    }


    BinaryFileReader::BinaryFileReader(std::ifstream&& file)
        : file_(std::move(file))
    {
    }


    size_t BinaryFileReader::remainingBytes() const
    {
        const auto current = file_.tellg();
        file_.seekg(0, std::ios::end);
        const auto end = file_.tellg();
        file_.seekg(current);
        return end - current;
    }


    std::expected<std::filesystem::path, std::string> findFileWithExtension(const std::filesystem::path& directoryPath, std::string_view extension)
    {
        if (!std::filesystem::exists(directoryPath))
            return std::unexpected(std::format("Directory not found: {}", directoryPath.string()));

        if (!std::filesystem::is_directory(directoryPath))
            return std::unexpected(std::format("Not a directory: {}", directoryPath.string()));

        for (const auto& entry : std::filesystem::directory_iterator(directoryPath))
        {
            if (entry.is_regular_file() && entry.path().extension() == extension)
                return entry.path();
        }

        return std::unexpected(std::format("No {} file found in: {}", extension, directoryPath.string()));
    }
}
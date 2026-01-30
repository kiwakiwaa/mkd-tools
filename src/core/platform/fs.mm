#include "monokakido/core/platform/fs.hpp"

#import <Foundation/Foundation.h>

#include <fstream>
#include <sstream>

namespace monokakido::platform::fs {


    std::expected<BinaryFileReader, std::string> BinaryFileReader::open(const std::filesystem::path& filePath)
    {
        std::ifstream file(filePath);
        if (!file)
            return std::unexpected(std::format("Failed to open file '{}'", filePath.string()));

        return BinaryFileReader(std::move(file));
    }


    std::expected<void, std::string> BinaryFileReader::seek(const size_t offset)
    {
        file_.seekg(static_cast<std::streamoff>(offset));
        if (!file_)
        {
            return std::unexpected(std::format("Failed to seek to offset '{}'",
                                               offset));
        }
        return {};
    }


    std::expected<void, std::string> BinaryFileReader::readBytes(std::span<uint8_t> buffer)
    {
        file_.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(buffer.size()));

        if (!file_)
            return std::unexpected(makeFilestreamError(file_, std::format("read {} bytes", buffer.size())));

        return {};
    }


    std::expected<std::vector<uint8_t>, std::string> BinaryFileReader::readBytes(const size_t count)
    {
        std::vector<uint8_t> data(count);
        file_.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(count));

        if (!file_)
            return std::unexpected(makeFilestreamError(file_, std::format("read {} bytes", count)));

        return data;
    }


    std::expected<std::string, std::string> BinaryFileReader::readBytesIntoString(const size_t size)
    {
        std::string data;
        data.resize(size);

        const auto bytesToRead = static_cast<std::streamsize>(size);
        file_.read(reinterpret_cast<char*>(data.data()), bytesToRead);

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
        return static_cast<size_t>(end - current);
    }


    std::expected<std::filesystem::path, std::string> getValidatedFilePath(const std::filesystem::path& directoryPath, std::string_view filename)
    {
        auto filePath = directoryPath / filename;

        if (!std::filesystem::exists(filePath))
            return std::unexpected(std::format("{} not found in: {}", filename, filePath.parent_path().string()));

        if (std::filesystem::is_directory(filePath))
            return std::unexpected(std::format("{} not a directory: {}", filename, filePath.string()));

        return filePath;
    }


    std::filesystem::path getContainerPathByGroupIdentifier(const std::string& groupIdentifier)
    {
        @autoreleasepool {
              NSString* groupId = [NSString stringWithUTF8String:groupIdentifier.c_str()];
              NSURL* containerURL = [[NSFileManager defaultManager] containerURLForSecurityApplicationGroupIdentifier:groupId];
              if (!containerURL)
                  return {};

              return std::filesystem::path{containerURL.path.UTF8String};
        }
    }

    std::expected<std::string, std::error_code> readTextFile(const std::filesystem::path& path)
    {
        std::error_code ec;
        if (!std::filesystem::exists(path, ec))
            return std::unexpected(ec ? ec : std::make_error_code(std::errc::no_such_file_or_directory));

        if (std::filesystem::is_directory(path))
            return std::unexpected(ec ? ec : std::make_error_code(std::errc::is_a_directory));

        const auto fileSize = std::filesystem::file_size(path, ec);
        if (ec)
            return std::unexpected(ec);

        std::ifstream file(path, std::ios::in | std::ios::binary);
        if (!file)
        {
            return std::unexpected(std::make_error_code(std::errc::io_error));
        }

        std::string content;
        content.resize(fileSize);

        if (!file.read(content.data(), static_cast<long>(fileSize)))
        {
            return std::unexpected(std::make_error_code(std::errc::io_error));
        }

        return content;
    }


    std::string makeFilestreamError(const std::ifstream& file, std::string_view context)
    {
        std::string error = std::format("Failed to {}: ", context);

        if (file.eof())
        {
            error += "unexpected end of file";
        }
        else if (file.bad())
        {
            std::error_code ec(errno, std::generic_category());
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
}
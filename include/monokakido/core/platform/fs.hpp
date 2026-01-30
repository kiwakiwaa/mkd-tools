//
// Caoimheにより 2026/01/14 に作成されました。
//

#pragma once

#include "monokakido/resource/common.hpp"

#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <string>
#include <vector>


namespace monokakido::platform::fs
{
    [[nodiscard]] std::expected<std::filesystem::path, std::string> getValidatedFilePath(
        const std::filesystem::path& directoryPath, std::string_view filename);

    [[nodiscard]] std::filesystem::path getContainerPathByGroupIdentifier(const std::string& groupIdentifier);

    [[nodiscard]] std::expected<std::string, std::error_code> readTextFile(const std::filesystem::path& path);

    [[nodiscard]] std::string makeFilestreamError(const std::ifstream& file, std::string_view context);


    class BinaryFileReader
    {
    public:
        static std::expected<BinaryFileReader, std::string> open(const std::filesystem::path& filePath);

        std::expected<void, std::string> seek(size_t offset);

        template<typename T>
        [[nodiscard]] std::expected<T, std::string> readStruct()
        {
            static_assert(resource::SwappableEndianness<T>,
                          "Type must inherit from BinaryStruct and implement swapEndianness()");

            T data{};
            file_.read(reinterpret_cast<char*>(&data), sizeof(T));

            if (!file_)
                return std::unexpected(makeFilestreamError(file_, std::format("read {} struct", typeid(T).name())));

            data.toLittleEndian();

            return data;
        }

        template<std::integral T>
        [[nodiscard]] std::expected<T, std::string> read()
        {
            T value{};
            file_.read(reinterpret_cast<char*>(&value), sizeof(T));

            if (!file_)
                return std::unexpected(makeFilestreamError(file_, std::format("read {}", typeid(T).name())));

            // Handle endianness for multi-byte types
            if constexpr (sizeof(T) > 1)
            {
                if constexpr (std::endian::native == std::endian::big)
                {
                    value = std::byteswap(value);
                }
            }

            return value;
        }

        template<typename T>
        [[nodiscard]] std::expected<std::vector<T>, std::string> readStructArray(size_t count)
        {
            static_assert(resource::SwappableEndianness<T>,
                          "Type must inherit from BinaryStruct and implement swapEndianness()");

            std::vector<T> data(count);
            const auto bytesToRead = static_cast<std::streamsize>(count * sizeof(T));
            file_.read(reinterpret_cast<char*>(data.data()), bytesToRead);

            if (!file_)
                return std::unexpected(makeFilestreamError(file_, std::format("read {} array", typeid(T).name())));

            for (auto& item : data)
                item.toLittleEndian();

            return data;
        }

        [[nodiscard]] std::expected<void, std::string> readBytes(std::span<uint8_t> buffer);

        [[nodiscard]] std::expected<std::vector<uint8_t>, std::string> readBytes(size_t count);

        [[nodiscard]] std::expected<std::string, std::string> readBytesIntoString(size_t size);

        [[nodiscard]] size_t remainingBytes() const;

    private:
        explicit BinaryFileReader(std::ifstream&& file);

        mutable std::ifstream file_;
    };
}

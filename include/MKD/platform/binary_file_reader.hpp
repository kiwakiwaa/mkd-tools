//
// kiwakiwaaにより 2026/01/14 に作成されました。
//

#pragma once

#include "MKD/resource/common.hpp"

#include <cstring>
#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <string>
#include <vector>

namespace MKD
{
    [[nodiscard]] std::expected<std::filesystem::path, std::string> findFileWithExtension(
        const std::filesystem::path& directoryPath, std::string_view extension);


    [[nodiscard]] std::string makeFilestreamError(const std::ifstream& file, std::string_view context);


    class BinaryFileReader
    {
    public:
        static std::expected<BinaryFileReader, std::string> open(const std::filesystem::path& filePath);

        std::expected<void, std::string> seek(size_t offset) const;

        template<typename T>
        [[nodiscard]] std::expected<T, std::string> readStruct()
        {
            static_assert(SwappableEndianness<T>, "Type must inherit from BinaryStruct and implement swapEndianness()");

            T data{};
            file_.read(reinterpret_cast<char*>(&data), sizeof(T));

            if (!file_)
                return std::unexpected(makeFilestreamError(file_, std::format("read {} struct", typeid(T).name())));

            data.toLittleEndian();

            return data;
        }

        template<typename T>
        [[nodiscard]] std::expected<T, std::string> readStructPartial(const size_t size)
        {
            static_assert(std::is_trivially_copyable_v<T>, "Type must be trivially copyable");

            T data{};
            const size_t bytesToRead = std::min(size, sizeof(T));

            auto bytesResult = readBytes(bytesToRead);
            if (!bytesResult)
                return std::unexpected(bytesResult.error());

            std::memcpy(&data, bytesResult->data(), bytesToRead);

            // For types that need endian swapping
            if constexpr (requires { data.swapEndianness(); })
            {
                if constexpr (std::endian::native == std::endian::big)
                {
                    data.swapEndianness();
                }
            }

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
            static_assert(SwappableEndianness<T>,
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

        [[nodiscard]] std::expected<void, std::string> readBytes(std::span<uint8_t> buffer) const;

        [[nodiscard]] std::expected<std::vector<uint8_t>, std::string> readBytes(size_t count) const;

        [[nodiscard]] std::expected<std::string, std::string> readBytesIntoString(size_t size) const;

        [[nodiscard]] size_t remainingBytes() const;

    private:
        explicit BinaryFileReader(std::ifstream&& file);

        mutable std::ifstream file_;
    };
}

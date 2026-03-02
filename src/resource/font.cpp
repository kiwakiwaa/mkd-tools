//
// kiwakiwaaにより 2026/02/01 に作成されました。
//

#include "MKD/resource/font.hpp"
#include "MKD/resource/rsc/rsc.hpp"

namespace MKD
{
    Result<Font> Font::load(const fs::path& directoryPath)
    {
        const auto fontName = directoryPath.stem().string();

        auto rscResult = Rsc::open(directoryPath);
        if (!rscResult)
            return std::unexpected(std::format("Failed to load font '{}': {}", fontName, rscResult.error()));

        const auto& rsc = rscResult.value();

        size_t totalSize = 0;
        for (auto&& [itemId, data] : rsc)
        {
            totalSize += data.size();
        }

        std::vector<uint8_t> fontData;
        fontData.reserve(totalSize);
        for (const auto& [itemId, data] : rsc)
        {
            fontData.insert(fontData.end(), data.begin(), data.end());
        }

        return Font(fontName, fontData);
    }


    const std::string& Font::name() const noexcept
    {
        return name_;
    }


    Result<std::span<const uint8_t>> Font::getData() const
    {
        return std::span(data_);
    }


    std::optional<std::string> Font::detectType() const
    {
        if (data_.empty() || data_.size() < 8)
            return std::nullopt;

        const uint32_t magic = (data_[0] << 24 |
                                data_[1] << 16 |
                                data_[2] << 8 |
                                data_[3]);

        if (magic == 0x4F54544F) // "OTTO"
            return "otf"; // OpenType

        if (magic == 0x00010000 || magic == 0x74727565) // TrueType or true
            return "ttf";

        return std::nullopt;
    }


    bool Font::isEmpty() const noexcept
    {
        return data_.empty();
    }


    Font::Font(std::string name, std::vector<uint8_t> data)
        : name_(std::move(name)), data_(std::move(data))
    {
    }
}

//
// kiwakiwaaにより 2026/01/22 に作成されました。
//

#include "monokakido/resource/common.hpp"


namespace monokakido::resource::detail
{
    std::optional<uint32_t> parseSequenceNumber(const fs::path& filename, std::string_view extension)
    {
        if (filename.extension() != extension)
            return std::nullopt;

        const auto stem = filename.stem().string();
        const size_t lastDash = stem.find_last_of('-');
        const bool hasDash = lastDash != std::string_view::npos;

        try
        {
            return hasDash
                       ? std::stoul(stem.substr(lastDash + 1)) - 1
                       : std::stoul(filename.string());
        }
        catch (...)
        {
            return std::nullopt;
        }
    }
}

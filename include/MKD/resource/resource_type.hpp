//
// kiwakiwaaにより 2026/02/21 に作成されました。
//

#pragma once

#include <array>
#include <cstdint>
#include <string_view>
#include <utility>


namespace MKD
{
    enum class ResourceType : uint8_t
    {
        Audio,
        Entries,
        Graphics,
        Fonts,
        Keystores,
        Headlines
    };

    constexpr std::string_view resourceTypeName(const ResourceType type)
    {
        switch (type)
        {
            case ResourceType::Audio:       return "Audio";
            case ResourceType::Entries:     return "Entries";
            case ResourceType::Graphics:    return "Graphics";
            case ResourceType::Fonts:       return "Fonts";
            case ResourceType::Keystores:   return "Keystores";
            case ResourceType::Headlines:   return "Headlines";
        }
        std::unreachable();
    }

    inline constexpr std::array kAllResourceTypes = {
        ResourceType::Audio,
        ResourceType::Entries,
        ResourceType::Graphics,
        ResourceType::Fonts,
        ResourceType::Keystores,
        ResourceType::Headlines
    };
}

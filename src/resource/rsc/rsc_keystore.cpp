//
// kiwakiwaaにより 2026/01/30 に作成されました。
//

#include "monokakido/resource/rsc/rsc_keystore.hpp"


namespace monokakido::resource
{
    RscKeyStore& RscKeyStore::instance()
    {
        static RscKeyStore instance;
        return instance;
    }


    std::optional<std::array<uint8_t, 32> > RscKeyStore::getKey(std::string_view dictId)
    {
        if (const auto it = KNOWN_KEYS.find(dictId); it != KNOWN_KEYS.end())
        {
            return it->second;
        }

        return std::nullopt;
    }



    const std::unordered_map<std::string_view, std::array<uint8_t, 32>>
    RscKeyStore::KNOWN_KEYS = {
        {
            "KJT",
            {
                0x0F, 0x47, 0x3E, 0x04, 0x38, 0xFA, 0x48, 0x80,
                0x54, 0x99, 0x94, 0x92, 0xF6, 0xBF, 0x82, 0xEE,
                0xD2, 0xF9, 0x7D, 0x7C, 0x0F, 0x46, 0x05, 0x7B,
                0x89, 0x4B, 0x05, 0xDA, 0x69, 0x5A, 0x7F, 0xC2
            }
        }
    };
}

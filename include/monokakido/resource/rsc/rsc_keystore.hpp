//
// kiwakiwaaにより 2026/01/30 に作成されました。
//

#pragma once

#include <array>
#include <optional>
#include <string_view>
#include <unordered_map>

namespace monokakido::resource
{

    class RscKeyStore
    {
    public:
        /**
         * Get rsc keystore instance
         */
        static RscKeyStore& instance();

        /**
         * Lookup key by dictionary ID
         * @param dictId Dictionary ID
         * @return 32 byte key for .rsc decryption
         */
        static std::optional<std::array<uint8_t, 32>> getKey(std::string_view dictId);


    private:
        /**
         * Private constructor - use instance()
         */
        RscKeyStore() = default;

        static const std::unordered_map<std::string_view, std::array<uint8_t, 32>> KNOWN_KEYS;

    };

}

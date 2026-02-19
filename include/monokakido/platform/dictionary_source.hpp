//
// kiwakiwaaにより 2026/02/19 に作成されました。
//

#pragma once

#include "monokakido/dictionary/common.hpp"

#include <expected>
#include <vector>


namespace monokakido
{
    class DictionarySource
    {
    public:

        virtual ~DictionarySource() = default;
        [[nodiscard]] virtual std::expected<std::vector<DictionaryInfo>, std::string> findAllAvailable() const = 0;
        [[nodiscard]] virtual std::expected<DictionaryInfo, std::string> findById(std::string_view dictId) const = 0;

    };
}
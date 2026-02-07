//
// kiwakiwaaにより 2026/02/07 に作成されました。
//

#pragma once


#include "common.hpp"
#include "base_exporter.hpp"
#include "monokakido/resource/rsc/rsc.hpp"
#include "monokakido/resource/nrsc/nrsc.hpp"
#include "monokakido/resource/font.hpp"

#include <expected>

namespace monokakido
{
    class ResourceExporter final : public BaseExporter
    {
    public:
        static std::expected<ExportResult, std::string> exportAll(const Rsc& rsc, const ExportOptions& options);

        static std::expected<ExportResult, std::string> exportAll(const Nrsc& nrsc, const ExportOptions& options);

        static std::expected<ExportResult, std::string> exportFont(const Font& font, const ExportOptions& options);

    private:

        static std::tuple<std::string, std::string, std::span<const uint8_t>> prepareRscItem(
            uint32_t itemId,
            std::span<const uint8_t> data,
            const ExportOptions& options,
            std::vector<uint8_t>& buffer);

        static bool isAudioData(std::span<const uint8_t> data);

    };

}
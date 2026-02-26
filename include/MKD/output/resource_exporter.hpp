//
// kiwakiwaaにより 2026/02/07 に作成されました。
//

#pragma once

#include "export_options.hpp"
#include "export_result.hpp"
#include "base_exporter.hpp"
#include "MKD/resource/rsc/rsc.hpp"
#include "MKD/resource/nrsc/nrsc.hpp"
#include "MKD/resource/font.hpp"

#include <expected>

namespace MKD
{
    class ResourceExporter final : public BaseExporter
    {
    public:
        static std::expected<ExportResult, std::string> exportAll(const Rsc& rsc, const ExportOptions& options, ResourceType type);

        static std::expected<ExportResult, std::string> exportAll(const Nrsc& nrsc, const ExportOptions& options, ResourceType type);

        static std::expected<ExportResult, std::string> exportFont(const Font& font, const ExportOptions& options);

    private:

        static std::vector<uint8_t> prettyPrintXml(std::span<const uint8_t> data);
    };
}
//
// kiwakiwaaにより 2026/02/01 に作成されました。
//

#include "monokakido/output/base_exporter.hpp"

#include <format>
#include <fstream>

namespace monokakido
{
    std::expected<void, std::string> BaseExporter::writeData(const std::span<const uint8_t> data, const fs::path& path)
    {
        // Write to temp file first
        const fs::path tempPath = path.string() + ".tmp";

        try
        {
            if (const auto parent = path.parent_path(); !parent.empty())
            {
                std::error_code ec;
                fs::create_directories(parent, ec);
                if (ec)
                    return std::unexpected(
                        std::format("Failed to create directory: {}", ec.message()));
            }

            std::ofstream file(tempPath, std::ios::binary);
            if (!file) return std::unexpected("Failed to open file for writing");

            file.write(reinterpret_cast<const char*>(data.data()), data.size());
            if (!file) return std::unexpected("Failed to write data");

            file.close();

            // Atomic rename
            std::error_code ec;
            fs::rename(tempPath, path, ec);
            if (ec)
            {
                fs::remove(tempPath, ec);
                return std::unexpected(std::format("Failed to finalise file: {}", ec.message()));
            }

            return {};
        }
        catch (const std::exception& e)
        {
            std::error_code ec;
            fs::remove(tempPath, ec);
            return std::unexpected(e.what());
        }
    }


    bool BaseExporter::shouldSkipExisting(const fs::path& path, const bool overwrite)
    {
        return !overwrite && fs::exists(path);
    }
}

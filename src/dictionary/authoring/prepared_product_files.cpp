//
// kiwakiwaaにより 2026/10/24 に作成されました。
//

#include "prepared_product_files.hpp"
#include "io/write_validation.hpp"
#include "io/output_file.hpp"
#include <sstream>

namespace MKD::detail
{
    Result<void> PreparedProductFiles::add(std::string_view path, std::span<const uint8_t> bytes)
    {
        if (auto valid = WriteValidation::relativePath(path); !valid)
            return valid;

        const auto target = contents_ / WriteValidation::fromUtf8(path);
        std::error_code error;
        if (files_.contains(path) || std::filesystem::exists(target, error))
            return std::unexpected("Duplicate generated product path: " + std::string(path));

        if (error)
            return std::unexpected(error.message());

        const auto next = files_.lower_bound(path);
        if (next != files_.end() && next->first.starts_with(std::string(path) + "/"))
            return std::unexpected("Conflicting generated product path: " + std::string(path));

        for (auto parent = target.parent_path(); parent != contents_ && !parent.empty(); parent = parent.parent_path())
        {
            if (files_.contains(parent.lexically_relative(contents_).generic_string()))
                return std::unexpected("Conflicting generated product path: " + std::string(path));

            const auto exists = std::filesystem::exists(parent, error);
            if (error)
                return std::unexpected(error.message());
            if (exists && !std::filesystem::is_directory(parent, error))
                return std::unexpected("Generated product parent is not a directory: " + parent.string());
            if (error)
                return std::unexpected(error.message());
        }
        files_.emplace(std::string(path), std::vector<uint8_t>(bytes.begin(), bytes.end()));
        return {};
    }

    Result<void> PreparedProductFiles::add(std::string_view path, const pugi::xml_document& document)
    {
        std::ostringstream stream;
        document.save(stream, "  ", pugi::format_default, pugi::encoding_utf8);
        const auto text = stream.str();
        return add(path, {reinterpret_cast<const uint8_t*>(text.data()), text.size()});
    }

    bool PreparedProductFiles::contains(std::string_view path) const
    {
        if (files_.contains(path))
            return true;

        std::error_code error;
        return std::filesystem::is_regular_file(contents_ / WriteValidation::fromUtf8(path), error);
    }

    Result<RetainedSpan> PreparedProductFiles::read(std::string_view path) const
    {
        if (const auto found = files_.find(path); found != files_.end())
            return RetainedSpan::copy(found->second);
        return RetainedSpan::map(contents_ / WriteValidation::fromUtf8(path));
    }

    Result<void> PreparedProductFiles::emit() const
    {
        for (const auto& [path, bytes] : files_)
        {
            const auto target = contents_ / WriteValidation::fromUtf8(path);
            std::error_code error;
            std::filesystem::create_directories(target.parent_path(), error);
            if (error)
                return std::unexpected(error.message());
            if (auto written = OutputFile::writeAll(target, bytes); !written)
                return written;
        }
        return {};
    }
}

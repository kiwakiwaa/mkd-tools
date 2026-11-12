//
// kiwakiwaaにより 2026/10/16 に作成されました。
//

#include "resource_files.hpp"

#include <algorithm>
#include <charconv>
#include <format>
#include <limits>

namespace MKD::detail
{
    Result<std::vector<std::filesystem::path>> resourcePaths(const std::filesystem::path& directory,
                                                             std::string_view extension, bool recursive,
                                                             bool directories)
    {
        std::vector<std::filesystem::path> paths;
        std::error_code ec;
        const auto scan = [&]<typename Iterator>(Iterator iterator) -> Result<void>
        {
            const Iterator end;
            for (; iterator != end; iterator.increment(ec))
            {
                if (ec)
                    break;

                const bool match = directories ? iterator->is_directory(ec) : iterator->is_regular_file(ec);
                if (ec)
                    break;

                if (match && (extension.empty() || iterator->path().extension() == extension))
                    paths.push_back(iterator->path());
            }
            if (ec)
                return std::unexpected(std::format("Cannot scan '{}': {}", directory.string(), ec.message()));
            return {};
        };
        auto status = recursive
                          ? scan(std::filesystem::recursive_directory_iterator(directory, ec))
                          : scan(std::filesystem::directory_iterator(directory, ec));
        if (!status)
            return std::unexpected(status.error());
        std::ranges::sort(paths);
        return paths;
    }

    Result<std::optional<std::filesystem::path>> findResourceFile(
        const std::filesystem::path& directory, std::string_view extension)
    {
        auto files = resourcePaths(directory, extension);
        if (!files)
            return std::unexpected(files.error());
        if (files->size() > 1)
            return std::unexpected(std::format("Multiple {} files in '{}'", extension, directory.string()));
        if (files->empty())
            return std::optional<std::filesystem::path>{};
        return std::optional(std::move(files->front()));
    }

    Result<uint32_t> ResourceFiles::sequenceNumber(const std::filesystem::path& path)
    {
        const auto stem = path.stem().string();
        const auto dash = stem.find_last_of('-');
        const auto digits = std::string_view(stem).substr(dash == std::string::npos ? 0 : dash + 1);
        uint32_t value = 0;
        const auto [end, error] = std::from_chars(digits.data(), digits.data() + digits.size(), value);
        if (error != std::errc{} || end != digits.data() + digits.size() ||
            (dash != std::string::npos && value == 0))
            return std::unexpected(std::format("Invalid resource sequence filename: {}", path.filename().string()));
        return dash == std::string::npos ? value : value - 1;
    }

    Result<ResourceFiles> ResourceFiles::open(const std::filesystem::path& directory, const std::string_view extension)
    {
        ResourceFiles files;
        auto paths = resourcePaths(directory, extension);
        if (!paths)
            return std::unexpected(paths.error());
        if (paths->empty())
            return std::unexpected(std::format("No {} files in '{}'", extension, directory.string()));
        for (const auto& path : *paths)
        {
            auto sequence = sequenceNumber(path);
            if (!sequence)
                return std::unexpected(sequence.error());
            auto mapping = MappedFile::open(path);
            if (!mapping)
                return std::unexpected(mapping.error());
            files.files_.push_back(File{
                .sequence = *sequence,
                .globalOffset = 0,
                .mapping = std::make_shared<const MappedFile>(std::move(*mapping)),
            });
        }
        std::ranges::sort(files.files_, {}, &File::sequence);
        size_t globalOffset = 0;
        for (size_t i = 0; i < files.files_.size(); ++i)
        {
            auto& file = files.files_[i];
            if (file.sequence != i)
                return std::unexpected(std::format("Missing or duplicate {} sequence at {} in '{}'", extension, i,
                                                   directory.string()));
            if (file.mapping->size() > std::numeric_limits<size_t>::max() - globalOffset)
                return std::unexpected("Resource file sizes overflow the address space");
            file.globalOffset = globalOffset;
            globalOffset += file.mapping->size();
        }
        return files;
    }

    Result<RetainedSpan> ResourceFiles::read(size_t sequence, size_t offset, size_t length) const
    {
        if (sequence >= files_.size())
            return std::unexpected(std::format("Resource file sequence {} out of range", sequence));
        const auto& mapping = files_[sequence].mapping;
        return mapping->slice(offset, length).transform([&mapping](std::span<const uint8_t> bytes)
        {
            return RetainedSpan(mapping, bytes);
        });
    }

    Result<RetainedSpan> ResourceFiles::readGlobal(size_t offset, size_t length) const
    {
        // Payloads use global offsets but must fit within a single mapped file.
        auto file = std::ranges::upper_bound(files_, offset, {}, &File::globalOffset);
        if (file == files_.begin())
            return std::unexpected("Global resource offset out of range");
        --file;
        return read(file->sequence, offset - file->globalOffset, length);
    }
}

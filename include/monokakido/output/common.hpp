//
// kiwakiwaaにより 2026/01/17 に作成されました。
//

#pragma once

#include <filesystem>
#include <functional>
#include <vector>

namespace fs = std::filesystem;

namespace monokakido
{
    struct ExportResult
    {
        size_t totalResources = 0;
        size_t exported = 0;
        size_t skipped = 0;
        size_t failed = 0;
        uint64_t totalBytes = 0;
        std::vector<std::string> errors;

        [[nodiscard]] bool hasErrors() const noexcept { return !errors.empty(); }
        [[nodiscard]] bool isSuccess() const noexcept { return failed == 0; }

        ExportResult& operator+=(const ExportResult& other)
        {
            totalResources += other.totalResources;
            exported += other.exported;
            skipped += other.skipped;
            failed += other.failed;
            totalBytes += other.totalBytes;
            errors.insert(errors.end(), other.errors.begin(), other.errors.end());
            return *this;
        }
    };

    struct ExportOptions
    {
        fs::path outputDirectory;
        bool overwriteExisting = false;
        bool createSubdirectories = true;

        // For entry content export
        bool prettyPrintXml = false;
    };
}
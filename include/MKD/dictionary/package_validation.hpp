#pragma once

#include "authoring_diagnostics.hpp"
#include "MKD/result.hpp"
#include <filesystem>

namespace MKD
{
    /** Checks static package structure and supported references without modifying files or fetching URLs.
     * Invalid package contents are report issues. Failure to inspect the supplied path is a Result error. */
    Result<ValidationReport> validatePackage(const std::filesystem::path& path, ValidationOptions options = {});
}

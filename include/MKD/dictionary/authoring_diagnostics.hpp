//
// kiwakiwaaにより 2026/10/24 に作成されました。
//

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include <compare>

namespace MKD
{
    /** Caller-owned labels copied into diagnostics, independent of package-relative resource paths. */
    struct SourceContext
    {
        std::string filename;
        std::string record;
        auto operator<=>(const SourceContext&) const = default;
    };

    /** Offsets and columns count UTF-8 bytes. Lines and columns start at one. */
    struct SourceLocation
    {
        size_t offset = 0;
        size_t length = 0;
        size_t line = 1;
        size_t column = 1;
        auto operator<=>(const SourceLocation&) const = default;
    };

    struct AuthoringOrigin
    {
        std::string operation;
        std::string subject;
        SourceContext context;
        std::optional<SourceLocation> location;
        [[nodiscard]] std::string describe() const;
        auto operator<=>(const AuthoringOrigin&) const = default;
    };

    enum class IssueSeverity
    {
        Error,
        Warning
    };

    struct AuthoringIssue
    {
        /** Stable machine-readable code. Human-readable messages may change. */
        std::string code;
        std::optional<uint16_t> dictionaryId;
        std::string dictionary;
        std::string subject;
        std::string message;
        size_t occurrences = 1;
        /** At most three distinct origins, selected in lexical order. */
        std::vector<AuthoringOrigin> origins;
        IssueSeverity severity = IssueSeverity::Error;
        [[nodiscard]] std::string describe() const;
    };

    struct ValidationOptions
    {
        size_t maxIssues = 100;
        bool warningsAsErrors = false;
        bool scanAllDocuments = false;
    };

    struct ValidationReport
    {
        std::vector<AuthoringIssue> issues;
        size_t totalIssues = 0;
        size_t errors = 0;
        size_t warnings = 0;
        /** Open children are a completion prerequisite, separate from invalid authoring input. */
        std::vector<std::string> pendingDictionaries;
        [[nodiscard]] bool valid() const { return errors == 0; }
        [[nodiscard]] bool ready() const { return valid() && pendingDictionaries.empty(); }
        [[nodiscard]] bool truncated() const { return totalIssues > issues.size(); }
        [[nodiscard]] std::string describe() const;
    };
}

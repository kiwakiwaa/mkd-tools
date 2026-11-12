//
// kiwakiwaaにより 2026/10/24 に作成されました。
//

#pragma once

#include "MKD/dictionary/authoring_diagnostics.hpp"
#include "MKD/dictionary/metadata.hpp"
#include <string_view>
#include "MKD/dictionary/content_analysis.hpp"

namespace MKD::detail
{
    class SourceMap;
    AuthoringOrigin locatedOrigin(AuthoringOrigin origin, std::optional<content::SourceRange> range,
                                  const SourceMap& map);
    std::string_view diagnosticCode(content::DiagnosticKind kind);
    IssueSeverity diagnosticSeverity(content::DiagnosticKind kind);
    AuthoringOrigin origin(std::string operation, std::string subject, SourceContext context = {});

    struct DependencyOrigins
    {
        size_t occurrences = 0;
        std::vector<AuthoringOrigin> samples;
        void add(AuthoringOrigin value);
    };

    class IssueCollector
    {
    public:
        explicit IssueCollector(const ValidationOptions options) : options_(options)
        {
        }

        void add(AuthoringIssue issue);
        void add(std::string code, std::string subject, std::string message, AuthoringOrigin source);
        void add(std::string code, const DictionaryContent& dictionary, std::string subject, std::string message,
                 const DependencyOrigins& origins = {});
        void pending(std::string dictionary);
        [[nodiscard]] ValidationReport take() &&;

    private:
        ValidationOptions options_;
        ValidationReport report_;
    };
}

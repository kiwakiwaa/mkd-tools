//
// kiwakiwaaにより 2026/10/24 に作成されました。
//

#include "authoring_diagnostics.hpp"
#include "dictionary/content/analysis_support.hpp"
#include <algorithm>
#include <tuple>

namespace MKD
{
    std::string AuthoringOrigin::describe() const
    {
        auto result = operation;
        if (!subject.empty())
            result += " " + subject;

        if (!context.filename.empty())
            result += " in " + context.filename;

        if (!context.record.empty())
            result += " [" + context.record + "]";

        if (location)
            result += " at byte " + std::to_string(location->offset) + " (line " + std::to_string(location->line) +
                ", byte column " + std::to_string(location->column) + ")";

        return result;
    }

    std::string AuthoringIssue::describe() const
    {
        std::string result = (severity == IssueSeverity::Warning ? "warning [" : "[") + code + "] ";
        if (!dictionary.empty())
            result += dictionary + ": ";

        result += message;
        for (const auto& sample : origins)
            result += "\n  referenced by " + sample.describe();

        if (occurrences > origins.size() && !origins.empty())
            result += "\n  " + std::to_string(occurrences) + " occurrences";

        return result;
    }

    std::string ValidationReport::describe() const
    {
        std::string result;
        for (const auto& issue : issues)
        {
            if (!result.empty())
                result += '\n';
            result += issue.describe();
        }

        if (truncated())
        {
            if (!result.empty())
                result += '\n';
            result += std::to_string(totalIssues - issues.size()) + " additional authoring issues omitted";
        }

        for (const auto& dictionary : pendingDictionaries)
        {
            if (!result.empty())
                result += '\n';
            result += "Finish dictionary " + dictionary + " before finishing the product";
        }

        return result;
    }
}

namespace MKD::detail
{
    AuthoringOrigin locatedOrigin(AuthoringOrigin origin, std::optional<content::SourceRange> range,
                                  const SourceMap& map)
    {
        if (range)
        {
            const auto [line, column] = map.lineColumn(range->offset);
            origin.location = SourceLocation{range->offset, range->length, line, column};
        }
        return origin;
    }

    std::string_view diagnosticCode(content::DiagnosticKind kind)
    {
        switch (kind)
        {
        case content::DiagnosticKind::NativeMarkup:
            return "native-article-profile";
        case content::DiagnosticKind::DuplicateIdentifier:
            return "duplicate-document-identifier";
        default:
            return "document-coverage";
        }
    }

    IssueSeverity diagnosticSeverity(content::DiagnosticKind kind)
    {
        return kind == content::DiagnosticKind::DuplicateIdentifier ? IssueSeverity::Error : IssueSeverity::Warning;
    }

    AuthoringOrigin origin(std::string operation, std::string subject, SourceContext context)
    {
        return {std::move(operation), std::move(subject), std::move(context), {}};
    }

    void DependencyOrigins::add(AuthoringOrigin value)
    {
        ++occurrences;
        const auto at = std::ranges::lower_bound(samples, value);
        if (at != samples.end() && *at == value)
            return;

        samples.insert(at, std::move(value));
        if (samples.size() > 3)
            samples.pop_back();
    }

    static auto identity(const AuthoringIssue& issue)
    {
        return std::tie(issue.dictionaryId, issue.dictionary, issue.code, issue.subject, issue.severity, issue.origins);
    }

    void IssueCollector::add(AuthoringIssue issue)
    {
        ++report_.totalIssues;
        if (issue.severity == IssueSeverity::Warning)
        {
            ++report_.warnings;
            if (options_.warningsAsErrors)
                ++report_.errors;
        }
        else
            ++report_.errors;

        const auto at = std::ranges::lower_bound(report_.issues, issue, [](const auto& left, const auto& right)
        {
            return identity(left) < identity(right);
        });

        report_.issues.insert(at, std::move(issue));
        if (report_.issues.size() > options_.maxIssues)
            report_.issues.pop_back();
    }

    void IssueCollector::add(std::string code, std::string subject, std::string message, AuthoringOrigin source)
    {
        add({
            .code = std::move(code),
            .subject = std::move(subject),
            .message = std::move(message),
            .origins = {std::move(source)}
        });
    }

    void IssueCollector::add(std::string code, const DictionaryContent& dictionary, std::string subject,
                             std::string message, const DependencyOrigins& origins)
    {
        auto samples = origins.samples;
        if (samples.empty())
            samples.push_back(origin("preflight", subject));

        add({
            .code = std::move(code),
            .dictionaryId = dictionary.dictionaryId,
            .dictionary = dictionary.identifier,
            .subject = std::move(subject),
            .message = std::move(message),
            .occurrences = origins.occurrences ? origins.occurrences : 1,
            .origins = std::move(samples)
        });
    }

    void IssueCollector::pending(std::string dictionary)
    {
        report_.pendingDictionaries.push_back(std::move(dictionary));
    }

    ValidationReport IssueCollector::take() &&
    {
        std::ranges::sort(report_.pendingDictionaries);
        return std::move(report_);
    }
}

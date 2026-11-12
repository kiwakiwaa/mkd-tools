#pragma once

#include "MKD/dictionary/content_analysis.hpp"
#include "MKD/dictionary/appendix_menu.hpp"
#include "MKD/dictionary/metadata.hpp"
#include "MKD/resource/retained_span.hpp"
#include "dictionary/authoring/authoring_diagnostics.hpp"
#include <functional>
#include <map>
#include <set>

namespace MKD::detail
{
    struct DocumentRequest
    {
        std::string path;
        std::optional<std::string> fragment;
        AuthoringOrigin origin;
    };

    class DocumentChecks
    {
    public:
        using Reader = std::function<Result<RetainedSpan>(std::string_view)>;
        using Exists = std::function<Result<void>(std::string_view)>;
        using Target = std::function<Result<void>(const AppendixEntryLink&)>;
        DocumentChecks(IssueCollector& issues, Reader read, Exists exists, Target target,
                       std::vector<DictionaryContent> dictionaries,
                       std::map<std::string, SourceContext, std::less<>> sources = {});
        void document(DocumentRequest request);
        void document(std::string path, std::optional<std::string> fragment, const DependencyOrigins& origins);
        void references(const content::Analysis& analysis, std::string_view input, std::string_view sourcePath,
                        AuthoringOrigin origin, std::optional<uint16_t> articleDictionary = {});
        void entry(const content::EntryReference& target, std::optional<uint16_t> articleDictionary,
                   const DependencyOrigins& origins);
        void run();
        void scan(const std::filesystem::path& contents);

    private:
        struct Parsed
        {
            std::set<std::string> ids;
            bool readable = false;
            bool identifiersComplete = true;
        };

        void issue(std::string code, std::string subject, std::string message, AuthoringOrigin origin,
                   IssueSeverity severity = IssueSeverity::Error) const;
        void reference(const content::Reference& reference, std::string_view sourcePath, AuthoringOrigin origin,
                       std::optional<uint16_t> articleDictionary);
        std::optional<std::string> resolve(std::string_view value, std::string_view sourcePath,
                                           AuthoringOrigin origin) const;
        void issue(std::string code, std::string subject, std::string message, const DependencyOrigins& origins,
                   IssueSeverity severity = IssueSeverity::Error) const;
        void checkDocument(const std::string& path, const std::optional<std::string>& fragment,
                           const DependencyOrigins& origins);
        void parse(std::string path);
        Result<void> available(std::string_view path);

        IssueCollector& issues_;
        Reader read_;
        Exists exists_;
        Target target_;
        std::vector<DictionaryContent> dictionaries_;
        std::map<std::string, SourceContext, std::less<>> sources_;
        std::vector<std::pair<std::string, std::optional<std::string>>> pending_;
        std::map<std::pair<std::string, std::optional<std::string>>, DependencyOrigins> requests_;
        std::map<std::string, Result<void>, std::less<>> availability_;
        std::map<std::string, Parsed, std::less<>> parsed_;
    };

    bool htmlDocument(std::string_view path);
}

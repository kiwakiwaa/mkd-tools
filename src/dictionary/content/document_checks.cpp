#include "document_checks.hpp"
#include "analysis_support.hpp"
#include "io/write_validation.hpp"
#include "dictionary/package/paths.hpp"
#include <algorithm>

namespace MKD::detail
{
    bool htmlDocument(std::string_view path)
    {
        const auto ext = WriteValidation::fromUtf8(path).extension().string();
        return ext == ".html" || ext == ".xhtml" || ext == ".htm" || ext == ".xml";
    }

    DocumentChecks::DocumentChecks(IssueCollector& issues, Reader read, Exists exists, Target target,
                                   std::vector<DictionaryContent> dictionaries,
                                   std::map<std::string, SourceContext, std::less<>> sources)
        : issues_(issues), read_(std::move(read)), exists_(std::move(exists)), target_(std::move(target)),
          dictionaries_(std::move(dictionaries)), sources_(std::move(sources))
    {
    }

    void DocumentChecks::issue(std::string code, std::string subject, std::string message, AuthoringOrigin origin,
                               const IssueSeverity severity) const
    {
        issues_.add({
            .code = std::move(code),
            .subject = std::move(subject),
            .message = std::move(message),
            .origins = {std::move(origin)},
            .severity = severity
        });
    }

    void DocumentChecks::issue(std::string code, std::string subject, std::string message,
                               const DependencyOrigins& origins, IssueSeverity severity) const
    {
        issues_.add({
            .code = std::move(code),
            .subject = std::move(subject),
            .message = std::move(message),
            .occurrences = origins.occurrences,
            .origins = origins.samples,
            .severity = severity
        });
    }

    void DocumentChecks::document(DocumentRequest request)
    {
        DependencyOrigins origins;
        origins.add(std::move(request.origin));
        document(std::move(request.path), std::move(request.fragment), origins);
    }

    void DocumentChecks::document(std::string path, std::optional<std::string> fragment,
                                  const DependencyOrigins& origins)
    {
        auto [found, inserted] = requests_.try_emplace({std::move(path), std::move(fragment)});
        if (inserted)
            pending_.push_back(found->first);

        auto& destination = found->second;
        const auto count = destination.occurrences + origins.occurrences;
        for (const auto& source : origins.samples)
            destination.add(source);

        destination.occurrences = count;
    }

    std::optional<std::string> DocumentChecks::resolve(std::string_view value, std::string_view sourcePath,
                                                       AuthoringOrigin origin) const
    {
        auto path = resolveDocumentPath(value, sourcePath);
        if (!path)
        {
            issue("invalid-document-reference", std::string(value), path.error(), origin);
            return {};
        }

        return *path;
    }

    void DocumentChecks::references(const content::Analysis& analysis, std::string_view input,
                                    std::string_view sourcePath, const AuthoringOrigin origin,
                                    std::optional<uint16_t> articleDictionary)
    {
        const SourceMap map(input);
        for (const auto& diagnostic : analysis.diagnostics)
            issue(std::string(diagnosticCode(diagnostic.kind)), std::string(sourcePath), diagnostic.message,
                  locatedOrigin(origin, diagnostic.source, map), diagnosticSeverity(diagnostic.kind));

        for (const auto& value : analysis.references)
            reference(value, sourcePath, locatedOrigin(origin, value.source, map), articleDictionary);
    }

    void DocumentChecks::entry(const content::EntryReference& target, std::optional<uint16_t> articleDictionary,
                               const DependencyOrigins& origins)
    {
        const auto dictionaryId =
            target.dictionaryId.value_or(0) ? *target.dictionaryId : articleDictionary.value_or(0);
        AuthoringIssue result{
            .subject = target.entryId.toString(), .occurrences = origins.occurrences, .origins = origins.samples
        };
        if (!dictionaryId)
        {
            result.code = "missing-entry-dictionary";
            result.message = "Entry links in a loose document need an explicit dictionary ID. A file document has no "
                "current article dictionary.";
        }
        else if (std::ranges::none_of(dictionaries_,
                                      [&](const auto& dictionary) { return dictionary.dictionaryId == dictionaryId; }))
        {
            result.code = "unverifiable-entry-dictionary";
            result.message =
                "Dictionary " + std::to_string(dictionaryId) +
                " is outside this package. Its entry target cannot be checked without the installed dictionary.";
            result.severity = IssueSeverity::Warning;
        }
        else
        {
            auto valid = target_({target.entryId, dictionaryId});
            if (valid)
                return;
            result.code = "missing-entry-target";
            result.message = valid.error();
        }
        issues_.add(std::move(result));
    }

    void DocumentChecks::reference(const content::Reference& reference, std::string_view sourcePath,
                                   AuthoringOrigin origin, std::optional<uint16_t> articleDictionary)
    {
        DependencyOrigins origins;
        origins.add(origin);
        if (const auto target = std::get_if<content::EntryReference>(&reference.target))
            entry(*target, articleDictionary, origins);
        else if (const auto document = std::get_if<content::DocumentReference>(&reference.target))
        {
            // CSS paint fragments belong to the rendered resource, rather than the stylesheet document.
            if (document->path.empty() && document->syntax == content::ResourceSyntax::CssUrl)
                return;
            if (document->path.empty() && articleDictionary)
            {
                if (const auto target = articleEntryReference(*document))
                    entry(*target, articleDictionary, origins);
                return;
            }

            const auto path = document->path.empty()
                                  ? std::optional(std::string(sourcePath))
                                  : resolve(document->path, sourcePath, origin);
            if (!path)
                return;

            auto fragment = document->fragment;
            if (fragment)
            {
                auto decoded = decodeUrlPath(*fragment);
                if (!decoded)
                {
                    issue("invalid-document-reference", *path, decoded.error(), origin);
                    return;
                }
                fragment = std::move(*decoded);
            }
            this->document({*path, std::move(fragment), std::move(origin)});
        }
        else if (const auto asset = std::get_if<content::AssetReference>(&reference.target))
        {
            auto value = std::string_view(asset->path);
            value = value.substr(0, value.find_first_of("?#"));
            auto path = resolve(value, sourcePath, origin);
            if (!path)
                return;

            if (auto found = available(*path); !found)
                issue("missing-document-resource", *path,
                      "Cannot read local resource '" + asset->path + "': " + found.error(), origin);
            else if (path->ends_with(".css") || htmlDocument(*path))
                this->document({*path, {}, std::move(origin)});
        }
        else if (const auto gaiji = std::get_if<content::GaijiReference>(&reference.target))
        {
            const auto prefix =
                WriteValidation::fromUtf8(sourcePath).parent_path().generic_string() + "/gaiji/" + gaiji->name;
            if (!available(prefix + ".png") && !available(prefix + ".svg"))
                issue("missing-gaiji", gaiji->name, "Missing gaiji " + gaiji->name, origin);
        }
    }

    void DocumentChecks::parse(std::string path)
    {
        if (parsed_.contains(path))
            return;

        auto& parsed = parsed_[path];
        auto bytes = read_(path);
        if (!bytes)
        {
            issue("unreadable-document", path, bytes.error(), origin("validateDocument", path));
            return;
        }

        auto text = bytes->utf8();
        auto context = sources_.contains(path) ? sources_.at(path) : SourceContext{};
        if (context.filename.empty())
            context.filename = path;

        auto source = origin("validateDocument", path, context);
        if (!text)
        {
            issue("invalid-document", path, text.error(), source);
            return;
        }
        if (path.ends_with(".css"))
        {
            auto analysis = content::analyzeCss(*text);
            if (!analysis)
            {
                issue("invalid-document-css", path, analysis.error().describe(path, *text), source);
                return;
            }
            analysis->diagnostics.clear();
            references(*analysis, *text, path, source);
        }
        else
        {
            auto analysis = content::analyzeDocument(*text);
            if (!analysis)
            {
                issue("invalid-document", path, analysis.error().describe(path, *text), source);
                return;
            }
            parsed.ids = std::move(analysis->identifiers);
            parsed.identifiersComplete = analysis->identifiersComplete;
            references(*analysis, *text, path, source);
        }
        parsed.readable = true;
    }

    void DocumentChecks::scan(const std::filesystem::path& contents)
    {
        std::error_code error;
        for (std::filesystem::recursive_directory_iterator it(contents, error), end; !error && it != end;
             it.increment(error))
            if (it->is_regular_file(error) && htmlDocument(it->path().generic_string()))
                document({
                    it->path().lexically_relative(contents).generic_string(),
                    {},
                    origin("scanDocuments", it->path().generic_string())
                });

        if (error)
            issue("document-scan-failed", "product", error.message(), origin("scanDocuments", "product"));
    }

    Result<void> DocumentChecks::available(std::string_view path)
    {
        if (const auto cached = availability_.find(path); cached != availability_.end())
            return cached->second;

        auto result = exists_(path);
        availability_.emplace(std::string(path), result);
        return result;
    }

    void DocumentChecks::checkDocument(const std::string& path, const std::optional<std::string>& fragment,
                                       const DependencyOrigins& origins)
    {
        if (auto valid = WriteValidation::relativePath(path); !valid)
        {
            issue("invalid-document-reference", path, valid.error(), origins);
            return;
        }

        if (auto found = available(path); !found)
        {
            issue("missing-document-resource", path, found.error(), origins);
            return;
        }

        if (!htmlDocument(path) && !path.ends_with(".css"))
        {
            if (fragment)
                issue("unsupported-document-fragment", path, "Only HTML documents support checked fragments.", origins);
            return;
        }

        const auto& parsed = parsed_.at(path);
        if (fragment && !fragment->empty() && parsed.readable && !parsed.ids.contains(*fragment))
            issue("missing-document-fragment", path + "#" + *fragment,
                  "Unknown document fragment '" + *fragment + "' in " + path +
                  (parsed.identifiersComplete ? "" : ". Fragment coverage is incomplete"),
                  origins, parsed.identifiersComplete ? IssueSeverity::Error : IssueSeverity::Warning);
    }

    void DocumentChecks::run()
    {
        // Discover the complete graph before reporting destinations, so later links keep their source origins.
        for (size_t index = 0; index < pending_.size(); ++index)
        {
            const auto key = pending_[index].first;
            if (WriteValidation::relativePath(key) && available(key) &&
                (htmlDocument(key) || key.ends_with(".css")))
            {
                parse(key);
            }
        }

        for (const auto& [destination, origins] : requests_)
            checkDocument(destination.first, destination.second, origins);
    }
}

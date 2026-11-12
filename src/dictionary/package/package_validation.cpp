#include "MKD/dictionary/package_validation.hpp"
#include "MKD/dictionary/dictionary_product.hpp"
#include "dictionary/content/document_checks.hpp"
#include "dictionary/package/paths.hpp"
#include "dictionary/package/package_format.hpp"
#include "dictionary/package/property_list.hpp"
#include "io/write_validation.hpp"
#include <set>

namespace MKD::detail
{
    class PackageValidator
    {
    public:
        PackageValidator(const DictionaryProduct& product, const ValidationOptions options)
            : product_(product), options_(options),
              contents_(product.paths().productRoot() / monokakido::paths::productContents), issues_(options),
              documents_(
                  issues_, [this](std::string_view path) { return read(path); },
                  [this](std::string_view path) { return exists(path); },
                  [this](const AppendixEntryLink link) { return target(link); }, product.metadata().contents)
        {
        }

        ValidationReport validate();

    private:
        void error(std::string code, std::string subject, std::string message);
        Result<RetainedSpan> read(std::string_view path) const;
        Result<void> exists(std::string_view path) const;
        Result<void> target(AppendixEntryLink link) const;
        void checkTarget(AppendixEntryLink link, std::string subject);
        void collectTargets(const Dictionary& dictionary);
        void checkArticles(const Dictionary& dictionary);
        void checkList(const AppendixList& list);
        void checkMenu(const AppendixMenu& menu);
        void checkNavigation(const NavigationView& view, size_t depth = 0);
        void checkInformation();
        void informationNode(pugi::xml_node node);
        const DictionaryProduct& product_;
        ValidationOptions options_;
        std::filesystem::path contents_;
        IssueCollector issues_;
        DocumentChecks documents_;
        std::map<uint16_t, std::set<EntryId>> targets_;
    };

    void PackageValidator::error(std::string code, std::string subject, std::string message)
    {
        issues_.add(std::move(code), subject, std::move(message), origin("validatePackage", subject, {subject, {}}));
    }

    Result<RetainedSpan> PackageValidator::read(std::string_view path) const
    {
        auto resolved = resolveProductPath(contents_, path);
        if (!resolved)
            return std::unexpected(resolved.error());

        for (const auto& dictionary : product_.dictionaries())
        {
            const auto prefix = dictionary.content().directory + "/";
            if (path.starts_with(prefix))
                return dictionary.resource(path.substr(prefix.size()));
        }
        return product_.resource(path);
    }

    Result<void> PackageValidator::exists(std::string_view path) const
    {
        auto resolved = resolveProductPath(contents_, path);
        if (!resolved)
            return std::unexpected(resolved.error());

        std::error_code error;
        if (std::filesystem::is_regular_file(*resolved, error))
            return {};
        if (error && error != std::errc::no_such_file_or_directory)
            return std::unexpected(error.message());

        for (const auto& dictionary : product_.dictionaries())
        {
            const auto prefix = dictionary.content().directory + "/";
            if (!path.starts_with(prefix))
                continue;

            const auto relative = path.substr(prefix.size());
            const auto slash = relative.find('/');
            if (slash == std::string_view::npos)
                break;

            const auto store = relative.substr(0, slash);
            const auto name = relative.substr(slash + 1);
            if (store == monokakido::paths::graphics || store == monokakido::paths::legacyGraphics)
            {
                const auto directory = contents_ / dictionary.content().directory;
                const auto preferred =
                    std::filesystem::is_directory(directory / monokakido::paths::graphics, error)
                        ? monokakido::paths::graphics
                        : monokakido::paths::legacyGraphics;
                if (store != preferred)
                {
                    auto alternate = NamedResourceStore::open(directory / WriteValidation::fromUtf8(store));
                    if (!alternate)
                        return std::unexpected(alternate.error());
                    return alternate->indexOf(name).transform([](size_t)
                    {
                    });
                }
                if (dictionary.graphics())
                    return dictionary.graphics()->indexOf(name).transform([](size_t)
                    {
                    });
            }
            if (store == monokakido::paths::audio && dictionary.audio())
                if (const auto audio = std::get_if<NamedResourceStore>(dictionary.audio()))
                    return audio->indexOf(name).transform([](size_t)
                    {
                    });
            break;
        }
        return std::unexpected("Missing local resource: " + std::string(path));
    }

    Result<void> PackageValidator::target(AppendixEntryLink link) const
    {
        if (!link.dictionaryId && !product_.dictionaries().empty())
            link.dictionaryId = product_.dictionaries().front().content().dictionaryId.value_or(0);

        const auto dictionary = targets_.find(link.dictionaryId);
        if (dictionary == targets_.end())
            return std::unexpected("Unknown target dictionary " + std::to_string(link.dictionaryId));

        if (!dictionary->second.contains(link.entryId))
            return std::unexpected("Missing entry target " + link.entryId.toString());

        return {};
    }

    void PackageValidator::checkTarget(const AppendixEntryLink link, std::string subject)
    {
        if (auto valid = target(link); !valid)
            error("invalid-appendix-target", std::move(subject), valid.error());
    }

    void PackageValidator::collectTargets(const Dictionary& dictionary)
    {
        auto& targets = targets_[dictionary.content().dictionaryId.value_or(0)];
        for (const auto item : dictionary.entryItems())
        {
            if (!item)
            {
                error("unreadable-article", dictionary.id(), item.error());
                continue;
            }

            auto text = item->data.utf8();
            if (!text)
            {
                error("invalid-article", dictionary.id(), text.error());
                continue;
            }

            auto analysis = content::analyzeArticle(item->itemId, *text);
            if (!analysis)
            {
                error("invalid-article", dictionary.id() + "/" + std::to_string(item->itemId),
                      analysis.error().describe("article " + std::to_string(item->itemId), *text));
                continue;
            }

            targets.insert(analysis->anchors.begin(), analysis->anchors.end());
        }
    }

    void PackageValidator::checkArticles(const Dictionary& dictionary)
    {
        const auto sourcePath = dictionary.content().directory + "/page.html";
        for (const auto item : dictionary.entryItems())
        {
            if (!item)
                continue;

            auto text = item->data.utf8();
            if (!text)
                continue;

            if (auto analysis = content::analyzeArticle(item->itemId, *text))
                documents_.references(
                    *analysis, *text, sourcePath,
                    origin("validateArticle", dictionary.id() + " page " + std::to_string(item->itemId),
                           {dictionary.content().directory + "/contents", std::to_string(item->itemId)}),
                    dictionary.content().dictionaryId);
        }

        if (const auto& stylesheet = dictionary.presentation().fullRubyStylesheet)
            documents_.document(
                {dictionary.content().directory + "/" + *stylesheet, {}, origin("presentation", dictionary.id())});
    }

    void PackageValidator::checkList(const AppendixList& list)
    {
        auto source = product_.openAppendixList(list);
        if (!source)
        {
            error("invalid-appendix-list", list.path, source.error());
            return;
        }

        for (size_t i = 0; i < source->size(); ++i)
        {
            auto entry = source->entryAt(i);
            if (!entry)
                error("invalid-appendix-list", list.path, entry.error());
            else
                checkTarget({entry->entryId, entry->dictionaryId}, list.path + " row " + std::to_string(i + 1));
        }
    }

    void PackageValidator::checkMenu(const AppendixMenu& menu)
    {
        for (const auto& section : menu.sections)
            for (const auto& item : section.items)
            {
                if (item.image && !product_.resource(*item.image))
                    error("missing-document-resource", *item.image, "Missing appendix thumbnail");

                std::visit([&](const auto& destination)
                    {
                        using T = std::decay_t<decltype(destination)>;
                        if constexpr (std::is_same_v<T, AppendixDocument>)
                            documents_.document(
                                {destination.path, destination.fragment, origin("appendices", item.name)});
                        else if constexpr (std::is_same_v<T, AppendixEntryLink>)
                            checkTarget(destination, item.name);
                        else if constexpr (std::is_same_v<T, AppendixList>)
                            checkList(destination);
                        else if constexpr (std::is_same_v<T, AppendixMenu>)
                            checkMenu(destination);
                        else if constexpr (std::is_same_v<T, AppendixGallery>)
                        {
                            for (const auto& image : destination.images)
                                if (!product_.resource(image))
                                    error("missing-document-resource", image, "Missing gallery image");
                        }
                        else
                            issues_.add({
                                .code = "document-coverage",
                                .subject = item.name,
                                .message = "Custom appendix presenter references are not checked.",
                                .severity = IssueSeverity::Warning
                            });
                    },
                    item.target);
            }
    }

    void PackageValidator::checkNavigation(const NavigationView& view, const size_t depth)
    {
        if (depth > 64)
        {
            error("invalid-navigation", "navigation", "Navigation is cyclic or excessively nested");
            return;
        }

        for (size_t s = 0; s < view.sectionCount(); ++s)
        {
            auto count = view.itemCount(s);
            if (!count)
            {
                error("invalid-navigation", "navigation", count.error());
                continue;
            }
            for (size_t i = 0; i < *count; ++i)
            {
                auto item = view.itemAt(s, i);
                if (!item)
                {
                    error("invalid-navigation", "navigation", item.error());
                    continue;
                }

                if (auto link = std::get_if<AppendixEntryLink>(&item->target))
                    checkTarget(*link, "navigation");
                else if (auto document = std::get_if<AppendixDocument>(&item->target))
                    documents_.document({document->path, document->fragment, origin("navigation", document->path)});
                else if (auto child = std::get_if<NavigationView>(&item->target))
                    checkNavigation(*child, depth + 1);
            }
        }
    }

    void PackageValidator::informationNode(pugi::xml_node node)
    {
        for (auto child : node.children())
        {
            if (std::string_view(child.name()) == "key" && std::string_view(child.child_value()) == "contents")
            {
                const auto value = child.next_sibling();
                if (std::string_view(value.name()) == "string")
                    documents_.document({value.child_value(), {}, origin("information", value.child_value())});
            }
            informationNode(child);
        }
    }

    void PackageValidator::checkInformation()
    {
        std::error_code filesystemError;
        const auto present = std::filesystem::exists(contents_ / monokakido::paths::moreInfo, filesystemError);
        if (filesystemError)
            error("invalid-product-information", monokakido::paths::moreInfo, filesystemError.message());
        else if (present)
        {
            auto information = PropertyList::open(contents_ / monokakido::paths::moreInfo, "array");
            if (!information)
                error("invalid-product-information", monokakido::paths::moreInfo, information.error());
            else
                informationNode(information->root());
        }
    }

    ValidationReport PackageValidator::validate()
    {
        // Two streaming passes keep article bodies out of the retained target catalogue.
        for (const auto& dictionary : product_.dictionaries())
            collectTargets(dictionary);

        for (const auto& dictionary : product_.dictionaries())
            checkArticles(dictionary);

        checkMenu(product_.appendices());

        auto navigation = product_.navigation();
        if (!navigation)
            error("invalid-navigation", "navigation", navigation.error());
        else if (*navigation)
            checkNavigation(**navigation);

        checkInformation();

        if (options_.scanAllDocuments)
            documents_.scan(contents_);

        documents_.run();
        return std::move(issues_).take();
    }
}

namespace MKD
{
    Result<ValidationReport> validatePackage(const std::filesystem::path& path, const ValidationOptions options)
    {
        std::error_code error;
        if (!std::filesystem::is_directory(path, error))
            return std::unexpected(error ? error.message() : "Package path is not a directory: " + path.string());

        auto product = DictionaryProduct::openAtPath(path);
        if (!product)
        {
            detail::IssueCollector issues(options);
            issues.add("invalid-package", path.filename().string(), product.error(),
                       detail::origin("validatePackage", path.string()));
            return std::move(issues).take();
        }
        return detail::PackageValidator(*product, options).validate();
    }
}

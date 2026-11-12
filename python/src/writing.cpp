//
// kiwakiwaaにより 2026/10/17 に作成されました。
//

#include "bindings.hpp"
#include "buffer.hpp"
#include <MKD/dictionary/product_writer.hpp>
#include <nanobind/stl/filesystem.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/string_view.h>
#include <nanobind/stl/vector.h>

namespace MKD::Python
{
    AppendixList addAppendixEntries(const DictionaryWriter& dictionary, std::string_view name, std::string_view title,
                                    const std::vector<AppendixEntry>& entries, const uint32_t rows,
                                    const uint32_t columns, SourceContext source)
    {
        return unwrap(
            dictionary.addAppendix(name, title, entries, {.rows = rows, .columns = columns}, std::move(source)));
    }

    template <typename T>
    static nb::typed<nb::tuple, T, nb::ellipsis> diagnosticTuple(const std::vector<T>& values)
    {
        nb::list result;
        for (const auto& value : values)
            result.append(nb::cast(value, nb::rv_policy::copy));

        return nb::tuple(result);
    }

    void bindCreationMetadata(nb::module_& module)
    {
        nb::class_<SourceContext>(module, "SourceContext")
            .def(nb::init<std::string, std::string>(), nb::kw_only(), "filename"_a = "", "record"_a = "")
            .def_ro("filename", &SourceContext::filename)
            .def_ro("record", &SourceContext::record);
        nb::class_<SourceLocation>(module, "SourceLocation")
            .def_ro("offset", &SourceLocation::offset)
            .def_ro("length", &SourceLocation::length)
            .def_ro("line", &SourceLocation::line)
            .def_ro("column", &SourceLocation::column);
        nb::class_<AuthoringOrigin>(module, "AuthoringOrigin")
            .def_ro("operation", &AuthoringOrigin::operation)
            .def_ro("subject", &AuthoringOrigin::subject)
            .def_ro("context", &AuthoringOrigin::context)
            .def_ro("location", &AuthoringOrigin::location)
            .def("describe", &AuthoringOrigin::describe);
        nb::enum_<IssueSeverity>(module, "IssueSeverity")
            .value("ERROR", IssueSeverity::Error).value("WARNING", IssueSeverity::Warning);
        nb::class_<AuthoringIssue>(module, "AuthoringIssue")
            .def_ro("code", &AuthoringIssue::code)
            .def_ro("severity", &AuthoringIssue::severity)
            .def_ro("dictionary_id", &AuthoringIssue::dictionaryId)
            .def_ro("dictionary", &AuthoringIssue::dictionary)
            .def_ro("subject", &AuthoringIssue::subject)
            .def_ro("message", &AuthoringIssue::message)
            .def_ro("occurrences", &AuthoringIssue::occurrences)
            .def_prop_ro("origins", [](const AuthoringIssue& issue)
            {
                return diagnosticTuple(issue.origins);
            }).def("describe", &AuthoringIssue::describe);
        nb::class_<ValidationOptions>(module, "ValidationOptions")
            .def(nb::init<size_t, bool, bool>(), nb::kw_only(), "max_issues"_a = 100,
                 "warnings_as_errors"_a = false, "scan_all_documents"_a = false)
            .def_ro("max_issues", &ValidationOptions::maxIssues)
            .def_ro("warnings_as_errors", &ValidationOptions::warningsAsErrors)
            .def_ro("scan_all_documents", &ValidationOptions::scanAllDocuments);
        nb::class_<ValidationReport>(module, "ValidationReport")
            .def_prop_ro("issues",
                         [](const ValidationReport& report)
                         {
                             return diagnosticTuple(report.issues);
                         })
            .def_ro("total_issues", &ValidationReport::totalIssues)
            .def_ro("errors", &ValidationReport::errors)
            .def_ro("warnings", &ValidationReport::warnings)
            .def_prop_ro("pending_dictionaries",
                         [](const ValidationReport& report)
                         {
                             return diagnosticTuple(report.pendingDictionaries);
                         })
            .def_prop_ro("valid", &ValidationReport::valid)
            .def_prop_ro("ready", &ValidationReport::ready)
            .def_prop_ro("truncated", &ValidationReport::truncated)
            .def("describe", &ValidationReport::describe);

        nb::enum_<CreationProfile>(module, "CreationProfile")
            .value("GENERAL", CreationProfile::General)
            .value("KANWA", CreationProfile::Kanwa);

        auto license = nb::class_<ProductLicense>(module, "ProductLicense");
        license.def(nb::init<std::string, std::string, std::optional<std::string>>(), "title"_a,
                    "text"_a = "", nb::kw_only(), "page"_a = nb::none());
        license.def_rw("title", &ProductLicense::title);
        license.def_rw("text", &ProductLicense::text);
        license.def_rw("page", &ProductLicense::page);

        auto information = nb::class_<ProductInformation>(module, "ProductInformation");
        information.def(nb::init<std::optional<LocalizedString>, std::vector<ProductLicense>,
                                 std::optional<std::string>>(), nb::kw_only(),
                        "copyright"_a = nb::none(), "licenses"_a = std::vector<ProductLicense>{},
                        "page"_a = nb::none());
        information.def_rw("copyright", &ProductInformation::copyright);
        information.def_rw("licenses", &ProductInformation::licenses);
        information.def_rw("page", &ProductInformation::page);
    }

    void bindProductWriter(nb::module_& module)
    {
        auto type = nb::class_<ProductWriter>(module, "ProductWriter",
                                              "Stage a new product. Finish each dictionary, then finish the product to publish.");
        type.def_static(
            "create", [](const std::filesystem::path& destination, DictionaryMetadata metadata, CreationProfile profile)
            {
                return unwrap(ProductWriter::create(destination, std::move(metadata), profile));
            }, "destination"_a,
            "metadata"_a, nb::kw_only(), "profile"_a = CreationProfile::General);

        type.def("add_dictionary",
                 [](const ProductWriter& product, DictionaryContent content, bool encrypt, SourceContext source)
                 {
                     return unwrap(product.addDictionary(std::move(content), {.encryptContents = encrypt}, source));
                 }, "content"_a, nb::kw_only(), "encrypt_contents"_a = false, "source"_a = SourceContext{},
                 nb::keep_alive<0, 1>());

        type.def("add_asset",
                 [](const ProductWriter& product, std::string_view path, nb::handle data, SourceContext source)
                 {
                     unwrap(product.addAsset(path, InputBytes(data).span(), source));
                 }, "path"_a, "data"_a, nb::kw_only(), "source"_a = SourceContext{});

        type.def("set_appendices", [](const ProductWriter& product, AppendixMenu menu, SourceContext source)
        {
            unwrap(product.setAppendices(std::move(menu), source));
        }, "menu"_a, nb::kw_only(), "source"_a = SourceContext{});

        type.def("set_navigation", [](const ProductWriter& product, NavigationIndex index, SourceContext source)
        {
            unwrap(product.setNavigation(std::move(index), source));
        }, "index"_a, nb::kw_only(), "source"_a = SourceContext{});

        type.def("set_information",
                 [](const ProductWriter& product, ProductInformation information, SourceContext source)
                 {
                     unwrap(product.setInformation(std::move(information), source));
                 }, "information"_a, nb::kw_only(), "source"_a = SourceContext{});

        type.def("set_icon",
                 [](const ProductWriter& product, nb::handle image, SourceContext source)
                 {
                     unwrap(product.setIcon(InputBytes(image).span(), source));
                 }, "image"_a, nb::kw_only(), "source"_a = SourceContext{},
                 "Generate the icon variants expected by Dictionaries from square PNG or JPEG bytes, "
                 "up to 4096 pixels wide.");

        type.def("preflight",
                 [](const ProductWriter& writer, ValidationOptions options)
                 {
                     return unwrap(writer.preflight(options));
                 }, nb::kw_only(), "options"_a = ValidationOptions{},
                 "Check authoring input without writing or sealing. Reports are immutable snapshots.");

        type.def("finish", [](const ProductWriter& product) { unwrap(product.finish()); },
                 "Publish the staged product at its new destination. Destruction never publishes.");
    }

    // Writer calls retain the GIL while borrowing Python buffers and mutating the build.
    void bindDictionaryWriter(nb::module_& module)
    {
        nb::enum_<HeadlineRole>(module, "HeadlineRole")
            .value("PRIMARY", HeadlineRole::Primary)
            .value("SECONDARY", HeadlineRole::Secondary)
            .value("SHORT", HeadlineRole::Short);

        auto type = nb::class_<DictionaryWriter>(module, "DictionaryWriter",
                                                 "Build one dictionary serially. Inputs are consumed during each call.");
        type.def("create_entry", [](nb::handle self, std::optional<uint32_t> pageId, SourceContext source)
        {
            return unwrap(nb::cast<const DictionaryWriter&>(self).createEntry(pageId, source));
        }, nb::kw_only(), "page_id"_a = nb::none(), "source"_a = SourceContext{}, nb::keep_alive<0, 1>());
        type.def("add_page",
                 [](const DictionaryWriter& dictionary, uint32_t pageId, std::string_view xml, SourceContext source)
                 {
                     unwrap(dictionary.addPage(pageId, xml, source));
                 }, "page_id"_a, "xml"_a, nb::kw_only(), "source"_a = SourceContext{});

        type.def(
            "add_headline",
            [](const DictionaryWriter& dictionary, HeadlineComponents headline, HeadlineRole role, SourceContext source)
            {
                unwrap(dictionary.addHeadline(std::move(headline), role, source));
            }, "headline"_a, nb::kw_only(), "role"_a = HeadlineRole::Primary, "source"_a = SourceContext{});

        type.def("add_search_key",
                 [](const DictionaryWriter& dictionary, SearchScope scope, std::string_view key, EntryId target,
                    bool exactOnly, SourceContext source)
                 {
                     unwrap(dictionary.addSearchKey(scope, key, target, {.exactOnly = exactOnly}, source));
                 },
                 "scope"_a, "key"_a, "target"_a, nb::kw_only(), "exact_only"_a = false, "source"_a = SourceContext{});

        type.def(
            "add_gaiji",
            [](const DictionaryWriter& dictionary, std::string_view filename, nb::handle data, SourceContext source)
            {
                return unwrap(dictionary.addGaiji(filename, InputBytes(data).span(), source));
            }, "filename"_a, "data"_a, nb::kw_only(), "source"_a = SourceContext{});

        type.def("add_graphic",
                 [](const DictionaryWriter& dictionary, std::string_view name, nb::handle data, SourceContext source)
                 {
                     unwrap(dictionary.addGraphic(name, InputBytes(data).span(), source));
                 }, "name"_a, "data"_a, nb::kw_only(), "source"_a = SourceContext{});

        type.def("add_audio",
                 [](const DictionaryWriter& dictionary, std::string_view name, nb::handle data, SourceContext source)
                 {
                     unwrap(dictionary.addAudio(name, InputBytes(data).span(), source));
                 }, "name"_a, "data"_a, nb::kw_only(), "source"_a = SourceContext{});

        type.def("add_asset",
                 [](const DictionaryWriter& dictionary, std::string_view path, nb::handle data, SourceContext source)
                 {
                     unwrap(dictionary.addAsset(path, InputBytes(data).span(), source));
                 }, "path"_a, "data"_a, nb::kw_only(), "source"_a = SourceContext{});

        type.def(
            "add_font",
            [](const DictionaryWriter& dictionary, std::string_view filename, nb::handle data, SourceContext source)
            {
                unwrap(dictionary.addFont(filename, InputBytes(data).span(), source));
            }, "filename"_a, "data"_a, nb::kw_only(), "source"_a = SourceContext{});

        type.def("set_presentation",
                 [](const DictionaryWriter& dictionary, DictionaryPresentation presentation, SourceContext source)
                 {
                     unwrap(dictionary.setPresentation(std::move(presentation), source));
                 }, "presentation"_a, nb::kw_only(), "source"_a = SourceContext{});

        type.def("set_entry_order",
                 [](const DictionaryWriter& dictionary, const std::vector<uint32_t>& ids, SourceContext source)
                 {
                     unwrap(dictionary.setEntryOrder(ids, source));
                 }, "page_ids"_a, nb::kw_only(), "source"_a = SourceContext{});

        type.def(
            "add_appendix",
            [](const DictionaryWriter& dictionary, std::string_view path, std::string_view title, SourceContext source)
            {
                return unwrap(dictionary.addAppendix(path, title, source));
            }, "path"_a, "title"_a, nb::kw_only(), "source"_a = SourceContext{});

        type.def("add_appendix_entries", &addAppendixEntries, "name"_a, "title"_a, "entries"_a, nb::kw_only(),
                 "rows"_a = 0, "columns"_a = 1, "source"_a = SourceContext{});

        type.def("preflight",
                 [](const DictionaryWriter& writer, ValidationOptions options)
                 {
                     return unwrap(writer.preflight(options));
                 }, nb::kw_only(), "options"_a = ValidationOptions{},
                 "Check authoring input without writing or sealing. Reports are immutable snapshots.");

        type.def("finish", [](const DictionaryWriter& dictionary) { unwrap(dictionary.finish()); },
                 "Validate references and complete this dictionary before publishing its product.");
    }
}

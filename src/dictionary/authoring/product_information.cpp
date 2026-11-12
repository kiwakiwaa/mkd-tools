#include "dictionary/content/document_checks.hpp"
//
// kiwakiwaaにより 2026/10/17 に作成されました。
//

#include "dictionary/authoring/product_information.hpp"
#include "product_information_style.hpp"
#include "dictionary/authoring/product_build.hpp"
#include "dictionary/package/property_list.hpp"
#include "dictionary/package/package_format.hpp"
#include "io/write_validation.hpp"

#include "prepared_product_files.hpp"
#include "authoring_diagnostics.hpp"
#include <sstream>

namespace MKD::detail
{
    class InformationPage
    {
    public:
        explicit InformationPage(std::string_view title, bool license = false)
        {
            auto html = document_.append_child("html");
            html.append_attribute("xmlns") = "http://www.w3.org/1999/xhtml";
            auto head = html.append_child("head");
            auto encoding = head.append_child("meta");
            encoding.append_attribute("http-equiv") = "Content-Type";
            encoding.append_attribute("content") = "text/html; charset=utf-8";
            head.append_child("title").text().set(std::string(title).c_str());
            // The Dictionaries information view applies linked stylesheets but ignores inline style blocks.
            stylesheet_ = head.append_child("link");
            stylesheet_.append_attribute("rel") = "stylesheet";
            stylesheet_.append_attribute("type") = "text/css";
            stylesheet_.append_attribute("href");
            auto body = html.append_child("body");
            main_ = body.append_child("main");
            main_.append_attribute("class") = license ? "license" : "product";
            append("h1", title);
        }

        void append(const char* element, std::string_view text, const char* className = nullptr) const
        {
            appendTo(main_, element, text, className);
        }

        void content(const DictionaryContent& content)
        {
            if (!content.title && !content.publisher && !content.edition)
                return;

            auto section = main_.append_child("section");
            section.append_attribute("class") = "content";
            if (content.title)
                appendTo(section, "h2", content.title->resolve());
            if (content.edition)
                appendTo(section, "p", content.edition->resolve(), "edition");
            if (content.publisher)
                appendTo(section, "p", content.publisher->resolve());
        }

        Result<void> prepare(const ProductBuild& product, PreparedProductFiles& files, std::string_view path) const
        {
            auto cssPath = WriteValidation::fromUtf8(path);
            cssPath.replace_extension(".css");
            const auto cssName = cssPath.filename().generic_string();
            stylesheet_.attribute("href").set_value(cssName.c_str());
            const auto css = resources::productInformationStylesheet;
            if (auto valid = product.validateAssetPath(path); !valid)
                return valid;
            if (auto valid = product.validateAssetPath(cssPath.generic_string()); !valid)
                return valid;
            if (auto written =
                    files.add(cssPath.generic_string(), {reinterpret_cast<const uint8_t*>(css.data()), css.size()});
                !written)
                return written;

            std::ostringstream stream;
            document_.save(stream, "  ", pugi::format_default, pugi::encoding_utf8);
            const auto text = stream.str();
            return files.add(path, {reinterpret_cast<const uint8_t*>(text.data()), text.size()});
        }

    private:
        static void appendTo(pugi::xml_node parent, const char* element, std::string_view text,
                             const char* className = nullptr)
        {
            if (text.empty())
                return;

            auto node = parent.append_child(element);
            if (className)
                node.append_attribute("class") = className;
            node.text().set(std::string(text).c_str());
        }

        pugi::xml_document document_;
        pugi::xml_node main_;
        pugi::xml_node stylesheet_;
    };

    class InformationDocument
    {
    public:
        static Result<void> validatePath(std::string_view path, bool allowPdf)
        {
            if (auto valid = WriteValidation::relativePath(path); !valid)
                return valid;

            if (auto valid = WriteValidation::xmlText(path); !valid)
                return valid;

            if (path.find_first_of("?#") != std::string_view::npos ||
                !(path.ends_with(".html") || path.ends_with(".xhtml") || (allowPdf && path.ends_with(".pdf"))))
                return std::unexpected("Information pages must use relative HTML or PDF asset paths");

            return {};
        }

        static Result<void> requireAsset(const PreparedProductFiles& files, std::string_view path)
        {
            if (!files.contains(path))
                return std::unexpected("Missing product information page: " + std::string(path));
            return {};
        }

        static void menuItem(pugi::xml_node items, std::string_view name, std::string_view path = {})
        {
            auto item = items.append_child("dict");
            property_list::string(item, monokakido::information::name, name);
            if (!path.empty())
                property_list::string(item, monokakido::information::contents, path);
        }

        static pugi::xml_node section(pugi::xml_node root)
        {
            auto section = root.append_child("dict");
            section.append_child("key").text().set(monokakido::information::items);
            return section.append_child("array");
        }

        static Result<void> productPage(const ProductBuild& product, const DictionaryMetadata& metadata,
                                        PreparedProductFiles& files)
        {
            const auto title = metadata.productTitle
                                   ? metadata.productTitle->resolve()
                                   : metadata.displayName
                                   ? metadata.displayName->resolve()
                                   : product.name();
            InformationPage page(title);
            if (metadata.description)
                page.append("p", metadata.description->resolve(), "description");

            for (const auto& content : metadata.contents)
                page.content(content);

            if (product.information().copyright)
                page.append("p", product.information().copyright->resolve(), "copyright");
            if (metadata.version)
                page.append("p", "Version " + *metadata.version, "version");

            return page.prepare(product, files, monokakido::paths::informationPage);
        }
    };

    Result<void> ProductInformationWriter::validate(const ProductInformation& information)
    {
        if (information.copyright)
            for (const auto* text : {&information.copyright->en, &information.copyright->ja})
                if (*text)
                    if (auto valid = WriteValidation::xmlText(**text, true); !valid)
                        return valid;

        if (information.page)
            if (auto valid = InformationDocument::validatePath(*information.page, false); !valid)
                return valid;

        for (const auto& license : information.licenses)
        {
            if (auto valid = WriteValidation::xmlText(license.title); !valid)
                return valid;

            if (license.page)
            {
                if (auto valid = InformationDocument::validatePath(*license.page, true); !valid)
                    return valid;
            }
            else if (auto valid = WriteValidation::xmlText(license.text); !valid)
                return valid;
        }
        return {};
    }

    void ProductInformationWriter::prepare(const ProductBuild& product, const DictionaryMetadata& metadata,
                                           PreparedProductFiles& files, IssueCollector& issues,
                                           DocumentChecks& documents,
                                           const SourceContext& context)
    {
        const auto check = [&](Result<void> result, std::string subject)
        {
            if (!result)
                issues.add("invalid-product-information", subject, result.error(),
                           origin("setInformation", subject, context));
        };
        const auto& information = product.information();
        const auto page = information.page.value_or(monokakido::paths::informationPage);
        if (information.page)
            check(InformationDocument::requireAsset(files, page), page);
        else
            check(InformationDocument::productPage(product, metadata, files), page);

        documents.document({page, {}, origin("setInformation", page, context)});
        pugi::xml_document document;
        auto root = property_list::root(document, "array");
        auto items = InformationDocument::section(root);
        InformationDocument::menuItem(items, monokakido::information::copyright, page);
        size_t index = 0;
        for (const auto& license : information.licenses)
        {
            const auto path = license.page.value_or("license/" + std::to_string(index++) + ".html");
            if (license.page)
                check(InformationDocument::requireAsset(files, path), path);
            else
            {
                InformationPage licensePage(license.title, true);
                licensePage.append("pre", license.text);
                check(licensePage.prepare(product, files, path), path);
            }
            documents.document({path, {}, origin("setInformation", path, context)});
            InformationDocument::menuItem(items, license.title, path);
        }
        InformationDocument::menuItem(InformationDocument::section(root), monokakido::information::settings);
        check(files.add(monokakido::paths::moreInfo, document), monokakido::paths::moreInfo);
    }
}

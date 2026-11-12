#include "dictionary/content/document_checks.hpp"
//
// kiwakiwaaにより 2026/10/18 に作成されました。
//

#include "dictionary/package/paths.hpp"
#include "dictionary/authoring/appendix_writer.hpp"
#include "dictionary/navigation/appendix_reader.hpp"
#include "dictionary/package/property_list.hpp"
#include "dictionary/authoring/product_build.hpp"
#include "dictionary/package/package_format.hpp"
#include "io/write_validation.hpp"
#include <algorithm>
#include <format>

namespace MKD::detail
{
    void AppendixWriter::prepare(const ProductBuild& product, const AppendixMenu& menu, PreparedProductFiles& files,
                                 IssueCollector& issues, DocumentChecks& documents, const SourceContext& context)
    {
        AppendixWriter writer(product, files, issues, documents, context);
        auto result = writer.menu(menu, monokakido::paths::appendixInfo);
        for (const auto& [target, origins] : writer.entryReferences_)
            if (auto valid = product.validateTarget({target.second, target.first}); !valid)
                issues.add({
                    .code = "invalid-appendix-target",
                    .dictionaryId = target.first,
                    .subject = target.second.toString(),
                    .message = valid.error(),
                    .occurrences = origins.occurrences,
                    .origins = origins.samples
                });
        if (!result)
            issues.add("invalid-appendices", "appendices", result.error(),
                       origin("setAppendices", "appendices", context));
    }

    Result<void> AppendixWriter::menu(const AppendixMenu& value, std::string_view path)
    {
        if (depth_ >= maximumAppendixMenuDepth)
            return std::unexpected("Appendix menus are excessively nested");
        ++depth_;

        pugi::xml_document document;
        auto sections = property_list::root(document, "array");
        for (const auto& section : value.sections)
        {
            if (auto valid = WriteValidation::xmlText(section.name, true); !valid)
            {
                issues_.add("invalid-appendix-section", section.name, valid.error(),
                            origin("setAppendices", section.name, context_));
                continue;
            }

            auto node = sections.append_child("dict");
            property_list::string(node, metadata::appendix::name, section.name);
            node.append_child("key").text().set(metadata::appendix::children);
            auto children = node.append_child("array");
            size_t row = 0;
            for (const auto& entry : section.items)
            {
                const auto previous = subject_;
                subject_ = std::string(path) + "/" + section.name + "/" + std::to_string(++row) + " " + entry.name;
                if (auto prepared = item(children.append_child("dict"), entry); !prepared)
                    issues_.add("invalid-appendix-item", subject_, prepared.error(),
                                origin("setAppendices", subject_, context_));
                subject_ = previous;
            }
        }
        --depth_;
        return files_.add(path, document);
    }

    Result<void> AppendixWriter::item(pugi::xml_node node, const AppendixItem& value)
    {
        if (auto valid = WriteValidation::xmlText(value.name, value.title.has_value() && value.number.has_value());
            !valid)
            return valid;

        property_list::string(node, metadata::appendix::name, value.name);
        const std::array fields{
            std::pair{metadata::appendix::title, &value.title},
            std::pair{metadata::appendix::number, &value.number},
            std::pair{metadata::appendix::description, &value.description},
        };

        for (const auto& [key, text] : fields)
            if (*text)
            {
                if (auto valid = WriteValidation::xmlText(**text); !valid)
                    return valid;

                property_list::string(node, key, **text);
            }

        if (value.image)
        {
            const auto extension = WriteValidation::fromUtf8(*value.image).extension().string();
            if (auto valid = file(*value.image, extension); !valid)
                return valid;

            property_list::string(node, metadata::appendix::image, *value.image);
        }

        if (value.indentation)
            property_list::integer(node, metadata::appendix::indentation, value.indentation);

        return std::visit(
            [&](const auto& destination)
            {
                return target(node, destination);
            },
            value.target);
    }

    Result<void> AppendixWriter::file(std::string_view path, std::string_view extension) const
    {
        if (auto valid = WriteValidation::xmlText(path); !valid)
            return valid;

        auto resolved = resolveProductPath(product_.path() / monokakido::paths::productContents, path);
        if (!resolved)
            return std::unexpected(resolved.error());

        if (resolved->extension() != extension)
            return std::unexpected("Invalid appendix file extension: " + std::string(path));

        if (!files_.contains(path))
            return std::unexpected("Missing appendix file: " + std::string(path));

        return {};
    }

    Result<void> AppendixWriter::target(pugi::xml_node node, const AppendixList& value) const
    {
        if (auto valid = file(value.path, ".entries"); !valid)
            return valid;

        const auto path =
            product_.path() / monokakido::paths::productContents / WriteValidation::fromUtf8(value.path);
        if (auto list = AppendixEntryList::open(path); !list)
            return std::unexpected(value.path + ": " + list.error());

        property_list::string(node, metadata::appendix::contents, value.path);
        property_list::boolean(node, metadata::appendix::singleSection, value.singleSection);
        property_list::boolean(node, metadata::appendix::usingAnchorId, value.usingAnchorId);
        property_list::boolean(node, metadata::appendix::skipThe, value.skipThe);
        return {};
    }

    Result<void> AppendixWriter::target(pugi::xml_node node, const AppendixDocument& value) const
    {
        const auto extension = WriteValidation::fromUtf8(value.path).extension().string();
        if (extension != ".html" && extension != ".pdf")
            return std::unexpected("Document appendices must be HTML or PDF assets");

        if (auto valid = file(value.path, extension); !valid)
            return valid;

        if (value.fragment)
        {
            if (extension != ".html")
                return std::unexpected("Only HTML appendix documents support fragments");
            if (auto valid = WriteValidation::xmlText(*value.fragment); !valid)
                return valid;
            if (value.fragment->contains('#'))
                return std::unexpected("Invalid appendix document fragment");
        }
        documents_.document({value.path, value.fragment, origin("setAppendices", subject_, context_)});
        property_list::string(node, metadata::appendix::contents,
                              value.path + (value.fragment ? "#" + *value.fragment : ""));
        return {};
    }

    Result<void> AppendixWriter::target(pugi::xml_node node, const AppendixEntryLink& value)
    {
        entryReferences_[{value.dictionaryId, value.entryId}].add(origin("setAppendices", subject_, context_));

        property_list::string(node, metadata::appendix::entry,
                              value.entryId.toString() + "-" + std::to_string(value.dictionaryId));
        return {};
    }

    Result<void> AppendixWriter::target(pugi::xml_node node, const AppendixMenu& value)
    {
        const auto path = std::format("{}/menu-{}.plist", monokakido::paths::appendixMenus, ++nextMenu_);
        if (auto written = menu(value, path); !written)
            return written;

        property_list::string(node, metadata::appendix::contents, path);
        return {};
    }

    Result<void> AppendixWriter::target(pugi::xml_node node, const AppendixGallery& value)
    {
        pugi::xml_document document;
        auto images = property_list::root(document, "array");
        for (const auto& image : value.images)
        {
            const auto extension = WriteValidation::fromUtf8(image).extension().string();
            if (auto valid = file(image, extension); !valid)
                return valid;

            property_list::string(images.append_child("dict"), metadata::appendix::file, image);
        }
        const auto relative =
            std::format("{}/gallery-{}/{}", monokakido::paths::appendixMenus, ++nextMenu_,
                        monokakido::paths::graphicsInfo);
        if (auto prepared = files_.add(relative, document); !prepared)
            return prepared;

        property_list::string(node, metadata::appendix::contents, relative);
        return {};
    }

    Result<void> AppendixWriter::target(pugi::xml_node, const AppendixCustom&)
    {
        return std::unexpected("App-specific appendix presenters cannot be authored");
    }
}

#include "dictionary/content/document_checks.hpp"
//
// kiwakiwaaにより 2026/10/19 に作成されました。
//

#include "dictionary/package/paths.hpp"
#include "dictionary/package/package_format.hpp"
#include "dictionary/authoring/navigation_writer.hpp"
#include "dictionary/navigation/index_format.hpp"
#include "dictionary/content/content_markup.hpp"
#include "dictionary/authoring/product_build.hpp"
#include "dictionary/package/property_list.hpp"
#include "MKD/resource/appendix_entry_list.hpp"
#include "io/write_validation.hpp"
#include <algorithm>
#include <limits>

namespace MKD::detail
{
    namespace nav = monokakido::navigation;

    nav::ItemKind NavigationWriter::Node::kind() const
    {
        if (const auto entries = std::get_if<EntryTable>(&content))
            return entries->labelled ? nav::ItemKind::LabelledEntry : nav::ItemKind::Entry;

        if (std::holds_alternative<Sections>(content))
            return nav::ItemKind::Section;

        if (std::holds_alternative<FileTable>(content))
            return nav::ItemKind::File;

        return nav::ItemKind::Reference;
    }

    NavigationWriter::NavigationWriter(const ProductBuild& product, PreparedProductFiles& files, IssueCollector& issues, DocumentChecks& documents,
                                       const SourceContext& context)
        : product_(product), files_(files), issues_(issues), documents_(documents), context_(context),
          contents_(product.path() / monokakido::paths::productContents),
          directory_(product.profile() == CreationProfile::Kanwa
                         ? product.firstContent().directory + "/" + nav::directory
                         : "navigation-menus"),
          kanwa_(product.profile() == CreationProfile::Kanwa)
    {
    }

    NavigationWriter::Encoder::Encoder(const uint32_t kind)
    {
        BinaryBuffer header(20);
        header.writeLittle(kind);
        header.writePadding(nav::headerSize);
        bytes_ = header.take();
    }

    void NavigationWriter::Encoder::patch(const size_t offset, const BinaryBuffer& record)
    {
        std::ranges::copy(record.data(), bytes_.begin() + static_cast<ptrdiff_t>(offset));
    }

    uint32_t NavigationWriter::Encoder::append(const BinaryBuffer& record, const size_t base)
    {
        const auto offset = static_cast<uint32_t>(bytes_.size() - base);
        bytes_.insert(bytes_.end(), record.data().begin(), record.data().end());
        return offset;
    }

    uint32_t NavigationWriter::Encoder::text(std::string_view value, const size_t base)
    {
        if (value.empty())
            return 0;

        const auto utf16 = unicode::toUtf16(value);
        BinaryBuffer record(4 + utf16.size() * 2);
        record.writeLittle(static_cast<uint32_t>(utf16.size() * 2));
        for (const auto unit : utf16)
            record.writeLittle(static_cast<uint16_t>(unit));

        return append(record, base);
    }

    Result<void> NavigationWriter::Encoder::node(const Node& value, const size_t base)
    {
        uint32_t title = 0;
        const auto* titleEntry = std::get_if<AppendixEntryLink>(&value.title);
        if (titleEntry)
        {
            BinaryBuffer record(8);
            record.writeLittle(titleEntry->entryId.pageId);
            record.writeLittle(titleEntry->entryId.itemId);
            record.writeLittle(titleEntry->dictionaryId);
            title = append(record, base);
        }
        else
            title = text(nav::encodeLabel(value.title), base);

        const auto data = bytes_.size();
        const auto kind = value.kind();
        size_t count = 0;
        if (kind == nav::ItemKind::Section)
        {
            const auto& children = std::get<Node::Sections>(value.content);
            count = children.size();
            bytes_.resize(data + count * nav::headerSize);
            for (size_t i = 0; i < count; ++i)
                if (auto result = node(children[i], data + i * nav::headerSize); !result)
                    return result;
        }
        else if (kind == nav::ItemKind::Entry || kind == nav::ItemKind::LabelledEntry)
        {
            const auto& entries = std::get<Node::EntryTable>(value.content).items;
            count = entries.size();
            const auto stride = nav::strides[static_cast<size_t>(kind)];
            bytes_.resize(data + count * stride);
            for (size_t i = 0; i < count; ++i)
            {
                const auto& item = entries[i];
                const auto& target = std::get<AppendixEntryLink>(item.target);
                BinaryBuffer record(stride);
                record.writeLittle(target.entryId.pageId);
                record.writeLittle(target.entryId.itemId);
                record.writeLittle(target.dictionaryId);
                if (kind == nav::ItemKind::LabelledEntry)
                    record.writeLittle(text(nav::encodeLabel(item.label), base));

                patch(data + i * stride, record);
            }
        }
        else if (kind == nav::ItemKind::File)
        {
            const auto& files = std::get<Node::FileTable>(value.content);
            count = files.size();
            bytes_.resize(data + count * 8);
            for (size_t i = 0; i < count; ++i)
            {
                BinaryBuffer record(8);
                record.writeLittle(text(files[i].first, base));
                record.writeLittle(text(files[i].second, base));
                patch(data + i * 8, record);
            }
        }
        else
        {
            const auto& reference = std::get<NavigationReference>(value.content);
            count = 1;
            bytes_.resize(data + 8);
            const auto file = text(reference.path, base);
            BinaryBuffer path(4 + reference.sectionPath.size() * 4);
            path.writeLittle(static_cast<uint32_t>(reference.sectionPath.size()));
            for (auto part : reference.sectionPath)
                path.writeLittle(part);

            const auto offset = append(path, base);
            BinaryBuffer record(8);
            record.writeLittle(file);
            record.writeLittle(offset);
            patch(data, record);
        }

        if (bytes_.size() > UINT32_MAX || count > UINT32_MAX)
            return std::unexpected("Navigation index is too large for the dictionary file format. "
                "Split it into smaller indexes.");

        BinaryBuffer header(nav::headerSize);
        header.writeByte(titleEntry ? 1 : 0);
        header.writeByte(static_cast<uint8_t>(kind));
        header.writeLittle(value.flags);
        header.writeLittle(title);
        header.writeLittle(static_cast<uint32_t>(count));
        header.writeLittle(static_cast<uint32_t>(data - base));
        patch(base, header);
        return {};
    }

    Result<void> NavigationWriter::label(const NavigationLabel& value, std::string_view subject)
    {
        if (const auto* link = std::get_if<AppendixEntryLink>(&value))
        {
            entry(*link, std::string(subject) + " label " + link->entryId.toString());
            return {};
        }

        if (const auto* radical = std::get_if<RadicalLabel>(&value))
        {
            if (!kanwa_)
                return std::unexpected("Radical labels require kanwa navigation");

            if (radical->character.empty())
                return std::unexpected("A radical label needs a character");

            for (const auto* text : {&radical->character, &radical->variants, &radical->reading})
                if (text->contains("／"))
                    return std::unexpected(
                        "Radical character, variant and reading fields must not contain the separator '／'");
        }

        return WriteValidation::xmlText(nav::encodeLabel(value), true);
    }

    AppendixEntryLink NavigationWriter::entry(AppendixEntryLink target, std::string subject)
    {
        if (!target.dictionaryId)
            target.dictionaryId = *product_.firstContent().dictionaryId;

        entryReferences_[{target.dictionaryId, target.entryId}].add(
            origin("setNavigation", std::move(subject), context_));
        return target;
    }

    Result<void> NavigationWriter::save(std::string_view path, std::span<const uint8_t> data) const
    {
        return files_.add(path, data);
    }

    std::string NavigationWriter::nextPath(std::string_view extension)
    {
        return directory_ + "/mkd-navigation-" + std::to_string(++next_) + std::string(extension);
    }

    std::string NavigationWriter::indexRelative(std::string_view path) const
    {
        return WriteValidation::fromUtf8(path)
               .lexically_relative(WriteValidation::fromUtf8(directory_))
               .generic_string();
    }

    Result<AppendixEntryList> NavigationWriter::entryList(const AppendixList& list) const
    {
        auto path = resolveProductPath(contents_, list.path);
        if (!path)
            return std::unexpected(list.path + ": " + path.error());

        if (path->extension() != ".entries")
            return std::unexpected("Entry lists must use .entries files: " + list.path);

        auto source = AppendixEntryList::open(*path);
        if (!source)
            return std::unexpected(list.path + ": " + source.error());

        return source;
    }

    Result<std::string> NavigationWriter::targetFile(const NavigationItem& item, size_t depth, std::string title)
    {
        if (const auto* child = std::get_if<NavigationIndex>(&item.target))
            return index(*child, title.empty() ? nav::plainLabel(item.label) : std::move(title), depth + 1);

        if (const auto* list = std::get_if<AppendixList>(&item.target))
        {
            auto source = entryList(*list);
            if (!source)
                return std::unexpected(source.error());

            if (!kanwa_)
                return list->path;

            NavigationIndex converted;
            converted.sections.emplace_back();
            converted.rows = source->rows();
            converted.columns = source->columns();
            for (size_t i = 0; i < source->size(); ++i)
            {
                auto value = source->entryAt(i);
                if (!value)
                    return std::unexpected(list->path + " row " + std::to_string(i + 1) + ": " + value.error());

                // The anchor option affects headline grouping, while row selection follows the full entry ID.
                converted.sections.front().items.push_back(
                    {std::string(value->label.value_or("")), AppendixEntryLink{value->entryId, value->dictionaryId}});
            }
            return index(converted, nav::plainLabel(item.label), depth + 1);
        }
        if (const auto* document = std::get_if<AppendixDocument>(&item.target))
        {
            auto path = resolveProductPath(contents_, document->path);
            if (!path)
                return std::unexpected(document->path + ": " + path.error());

            if (!files_.contains(document->path))
                return std::unexpected("Missing navigation document: " + document->path);

            const auto extension = path->extension();
            if (extension != ".html" && extension != ".xml" && extension != ".pdf")
                return std::unexpected("Navigation documents must be HTML, XML or PDF: " + document->path);

            documents_.document({document->path, document->fragment, origin("setNavigation", title, context_)});
            return document->path + (document->fragment ? "#" + *document->fragment : "");
        }
        if (const auto* reference = std::get_if<NavigationReference>(&item.target))
        {
            if (auto view = NavigationView::open(contents_, *reference); !view)
                return std::unexpected(reference->path + ": " + view.error());

            if (reference->sectionPath.empty())
                return reference->path;

            if (!kanwa_)
                return std::unexpected("Subtree references require kanwa navigation");

            Node node;
            node.title = nav::plainLabel(item.label);
            node.content = NavigationReference{indexRelative(reference->path), reference->sectionPath};
            Encoder encoder(0);
            if (auto encoded = encoder.node(node, 4); !encoded)
                return std::unexpected(encoded.error());

            const auto path = nextPath(".idx");
            if (auto written = save(path, encoder.bytes()); !written)
                return std::unexpected(written.error());

            return path;
        }

        if (std::holds_alternative<std::monostate>(item.target))
            return std::string{};

        return std::unexpected("An article target cannot be used as a navigation file");
    }

    Result<NavigationWriter::Node> NavigationWriter::section(const NavigationSection& value, size_t depth,
                                                             NavigationKind kind, std::string_view subject,
                                                             size_t firstItem)
    {
        Node result;
        result.title = value.title;
        const bool entries =
            !value.items.empty() && std::holds_alternative<AppendixEntryLink>(value.items.front().target);
        if (entries)
            result.content = Node::EntryTable{};
        else
            result.content = Node::FileTable{};

        size_t row = firstItem;
        for (const auto& item : value.items)
        {
            const auto itemSubject = std::string(subject) + "/item " + std::to_string(++row) + " " +
                nav::plainLabel(item.label);
            if (auto valid = label(item.label, itemSubject); !valid)
            {
                issues_.add("invalid-navigation-label", nav::plainLabel(item.label), valid.error(),
                            origin("setNavigation", itemSubject, context_));
                continue;
            }

            if (entries)
            {
                auto copy = item;
                copy.target = entry(std::get<AppendixEntryLink>(item.target), itemSubject);
                std::get<Node::EntryTable>(result.content).items.push_back(std::move(copy));
                if (!nav::encodeLabel(item.label).empty())
                    std::get<Node::EntryTable>(result.content).labelled = true;
            }
            else
            {
                const auto title = kind == NavigationKind::TotalStrokes ? nav::plainLabel(item.label) + "画" : "";
                auto path = targetFile(item, depth, title);
                if (!path)
                {
                    issues_.add("invalid-navigation-item", nav::plainLabel(item.label), path.error(),
                                origin("setNavigation", itemSubject, context_));
                    continue;
                }

                std::get<Node::FileTable>(result.content)
                    .emplace_back(nav::encodeLabel(item.label), path->empty() ? "" : indexRelative(*path));
            }
        }
        return result;
    }

    Result<NavigationWriter::Node> NavigationWriter::binary(const NavigationIndex& value, std::string title,
                                                            size_t depth)
    {
        Node root;
        root.title = std::move(title);
        // Binary navigation record tables are homogeneous. Split mixed menus into adjacent display sections.
        size_t sectionNumber = 0;
        for (const auto& original : value.sections)
        {
            const auto subject = nav::plainLabel(root.title) + "/section " + std::to_string(++sectionNumber) + " " +
                original.title;
            size_t firstItem = 0;
            if (auto valid = WriteValidation::xmlText(original.title, true); !valid)
                return std::unexpected(valid.error());

            NavigationSection run{original.title, {}};
            for (const auto& item : original.items)
            {
                if (!run.items.empty() && std::holds_alternative<AppendixEntryLink>(run.items.back().target) !=
                    std::holds_alternative<AppendixEntryLink>(item.target))
                {
                    auto node = section(run, depth, value.kind, subject, firstItem);
                    if (!node)
                        return std::unexpected(node.error());

                    std::get<Node::Sections>(root.content).push_back(std::move(*node));
                    firstItem += run.items.size();
                    run = {};
                }
                run.items.push_back(item);
            }

            auto node = section(run, depth, value.kind, subject, firstItem);
            if (!node)
                return std::unexpected(node.error());

            std::get<Node::Sections>(root.content).push_back(std::move(*node));
        }

        if (std::get<Node::Sections>(root.content).size() == 1 &&
            std::get<std::string>(std::get<Node::Sections>(root.content).front().title).empty())
        {
            auto node = std::move(std::get<Node::Sections>(root.content).front());
            node.title = std::move(root.title);
            root = std::move(node);
        }
        else
            root.flags = nav::sections | nav::gridHeaders | nav::listHeaders | nav::columnBreaks;

        if (value.compact)
            root.flags |= nav::compact;

        const auto* sections = std::get_if<Node::Sections>(&root.content);
        const bool unnamedSection = sections && std::ranges::any_of(*sections,
                                                                    [](const Node& child)
                                                                    {
                                                                        const auto* title =
                                                                            std::get_if<std::string>(&child.title);
                                                                        return title && title->empty();
                                                                    });
        if (!value.sectionIndex || unnamedSection)
            root.flags |= nav::hideSectionIndex;
        if (value.parentHeadlines)
            root.flags |= nav::parentHeadlines;

        return root;
    }

    Result<void> NavigationWriter::plistItem(pugi::xml_node parent, const NavigationItem& value, size_t depth,
                                             std::string_view subject)
    {
        if (auto valid = label(value.label, subject); !valid)
            return valid;

        auto node = parent.append_child("dict");
        property_list::string(node, "title", nav::plainLabel(value.label));
        if (const auto* target = std::get_if<AppendixEntryLink>(&value.target))
        {
            const auto link = entry(*target, std::string(subject));
            property_list::string(node, "entry", link.entryId.toString() + "-" + std::to_string(link.dictionaryId));
        }
        else if (!std::holds_alternative<std::monostate>(value.target))
        {
            auto path = targetFile(value, depth);
            if (!path)
                return std::unexpected(path.error());

            property_list::string(node, "file", *path);
            if (const auto* child = std::get_if<NavigationIndex>(&value.target))
            {
                property_list::string(node, "orientation", child->vertical ? "vertical" : "horizontal");
                property_list::boolean(node, "shorten", child->shortenHeadlines);
            }
        }
        return {};
    }

    Result<void> NavigationWriter::plist(const NavigationIndex& value, std::string_view title, std::string_view path,
                                         size_t depth)
    {
        pugi::xml_document document;
        auto node = property_list::root(document, "dict");
        property_list::string(node, "title", title);
        if (value.rows)
            property_list::integer(node, "row", value.rows);

        if (value.columns)
            property_list::integer(node, "col", value.columns);

        node.append_child("key").text().set("children");
        auto array = node.append_child("array");
        size_t sectionNumber = 0;
        for (const auto& section : value.sections)
        {
            const auto subject = std::string(path) + "/" + std::string(title) + "/section " +
                std::to_string(++sectionNumber) + " " + section.title;
            if (section.title.empty())
            {
                size_t row = 0;
                for (const auto& item : section.items)
                {
                    const auto itemSubject = subject + "/item " + std::to_string(++row) + " " +
                        nav::plainLabel(item.label);
                    if (auto result = plistItem(array, item, depth, itemSubject); !result)
                        issues_.add("invalid-navigation-item", nav::plainLabel(item.label), result.error(),
                                    origin("setNavigation", itemSubject, context_));
                }
            }
            else
            {
                NavigationIndex child;
                child.sections.push_back({"", section.items});
                if (auto result = plistItem(array, {section.title, child}, depth, subject); !result)
                    issues_.add("invalid-navigation-section", section.title, result.error(),
                                origin("setNavigation", subject, context_));
            }
        }

        return files_.add(path, document);
    }

    Result<std::string> NavigationWriter::index(const NavigationIndex& value, std::string title, size_t depth,
                                                std::optional<std::string> filename)
    {
        if (depth >= nav::maximumDepth)
            return std::unexpected("Navigation is excessively nested");

        const auto path = filename.value_or(nextPath(kanwa_ ? ".idx" : ".plist"));
        if (kanwa_)
        {
            auto node = binary(value, title, depth);
            if (!node)
                return std::unexpected(node.error());

            Encoder encoder(static_cast<uint32_t>(value.kind));
            if (auto encoded = encoder.node(*node, 4); !encoded)
                return std::unexpected(encoded.error());

            if (auto written = save(path, encoder.bytes()); !written)
                return std::unexpected(written.error());

            if (value.kind != NavigationKind::Generic && value.kind != NavigationKind::Root)
                types_.try_emplace(static_cast<uint32_t>(value.kind), indexRelative(path), std::move(title));
        }
        else if (auto written = plist(value, title, path, depth); !written)
            return std::unexpected(written.error());

        return path;
    }

    Result<std::string> NavigationWriter::gallery(const AppendixGallery& value)
    {
        std::string html =
            "<html xmlns=\"http://www.w3.org/1999/xhtml\" hmhtml=\"5\"><head><title>Images</title></head><body>";
        for (const auto& image : value.images)
        {
            const auto relative = indexRelative(image);
            html += "<p><img src=\"" + ContentMarkup::escapeXml(relative) + "\"/></p>";
        }

        html += "</body></html>";
        const auto path = nextPath(".html");
        if (auto saved = save(path, std::span(reinterpret_cast<const uint8_t*>(html.data()), html.size())); !saved)
            return std::unexpected(saved.error());

        return path;
    }

    Result<NavigationIndex> NavigationWriter::appendices(const AppendixMenu& menu, const size_t depth)
    {
        if (depth >= nav::maximumDepth)
            return std::unexpected("Appendix menu is excessively nested");

        NavigationIndex result;
        for (const auto& section : menu.sections)
        {
            NavigationSection converted{section.name, {}};
            for (const auto& item : section.items)
            {
                NavigationItem value{item.name, {}};
                auto translated = std::visit(
                    [&](const auto& target) -> Result<void>
                    {
                        using T = std::decay_t<decltype(target)>;
                        if constexpr (std::is_same_v<T, AppendixMenu>)
                        {
                            auto child = appendices(target, depth + 1);
                            if (!child)
                                return std::unexpected(child.error());

                            value.target = std::move(*child);
                        }
                        else if constexpr (std::is_same_v<T, AppendixGallery>)
                        {
                            auto path = gallery(target);
                            if (!path)
                                return std::unexpected(path.error());

                            value.target = AppendixDocument{*path, {}};
                        }
                        else if constexpr (std::is_same_v<T, AppendixCustom>)
                            return std::unexpected(
                                "Custom appendix presenters cannot be composed into generic navigation");
                        else
                            value.target = target;

                        return {};
                    },
                    item.target);

                if (!translated)
                    return std::unexpected(translated.error());

                converted.items.push_back(std::move(value));
            }
            result.sections.push_back(std::move(converted));
        }
        return result;
    }

    void NavigationWriter::prepare(const ProductBuild& product, const NavigationIndex& value,
                                   const AppendixMenu& appendices, PreparedProductFiles& files, IssueCollector& issues, DocumentChecks& documents,
                                   const SourceContext& context)
    {
        NavigationWriter writer(product, files, issues, documents, context);
        auto result = [&]() -> Result<void>
        {
            auto menu = value;
            if (!appendices.sections.empty())
            {
                auto converted = writer.appendices(appendices, 0);
                if (!converted)
                    return std::unexpected(converted.error());
                menu.sections.push_back({writer.kanwa_ ? "付録" : "", {{std::string("付録"), std::move(*converted)}}});
            }
            if (writer.kanwa_)
                menu.kind = NavigationKind::Root;
            auto prepared =
                writer.index(menu, "インデックス", 0,
                             writer.kanwa_ ? writer.directory_ + "/" + nav::rootIndex : std::string(nav::rootPlist));
            if (!prepared)
                return std::unexpected(prepared.error());
            if (writer.kanwa_)
            {
                pugi::xml_document document;
                auto root = property_list::root(document, "dict");
                for (const auto& [kind, info] : writer.types_)
                {
                    root.append_child("key").text().set(std::to_string(kind).c_str());
                    auto node = root.append_child("dict");
                    property_list::string(node, "fileName", info.first);
                    property_list::string(node, "title", info.second);
                }
                return files.add(writer.directory_ + "/" + nav::typeInfo, document);
            }
            return {};
        }();
        for (const auto& [target, origins] : writer.entryReferences_)
            if (auto valid = product.validateTarget({target.second, target.first}); !valid)
                issues.add({
                    .code = "invalid-navigation-target",
                    .dictionaryId = target.first,
                    .subject = target.second.toString(),
                    .message = valid.error(),
                    .occurrences = origins.occurrences,
                    .origins = origins.samples
                });
        if (!result)
            issues.add("invalid-navigation", "navigation", result.error(),
                       origin("setNavigation", "navigation", context));
    }
}

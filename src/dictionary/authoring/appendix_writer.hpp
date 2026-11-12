//
// kiwakiwaaにより 2026/10/18 に作成されました。
//

#pragma once

#include "MKD/dictionary/appendix_menu.hpp"
#include "MKD/result.hpp"
#include "authoring_diagnostics.hpp"
#include "prepared_product_files.hpp"
#include <filesystem>
#include <pugixml.hpp>

namespace MKD::detail
{
    class DocumentChecks;
    class ProductBuild;

    class AppendixWriter
    {
    public:
        static void prepare(const ProductBuild& product, const AppendixMenu& menu, PreparedProductFiles& files,
                            IssueCollector& issues, DocumentChecks& documents, const SourceContext& context);

    private:
        explicit AppendixWriter(const ProductBuild& product, PreparedProductFiles& files, IssueCollector& issues,
                                DocumentChecks& documents,
                                const SourceContext& context)
            : product_(product), files_(files), issues_(issues), documents_(documents), context_(context)
        {
        }

        Result<void> menu(const AppendixMenu& menu, std::string_view path);
        Result<void> item(pugi::xml_node node, const AppendixItem& item);
        Result<void> target(pugi::xml_node node, const AppendixList& list) const;
        Result<void> target(pugi::xml_node node, const AppendixDocument& document) const;
        Result<void> target(pugi::xml_node node, const AppendixEntryLink& entry);
        Result<void> target(pugi::xml_node node, const AppendixMenu& submenu);
        static Result<void> target(pugi::xml_node node, const AppendixCustom& custom);
        Result<void> target(pugi::xml_node node, const AppendixGallery& gallery);
        Result<void> file(std::string_view path, std::string_view extension) const;
        const ProductBuild& product_;
        PreparedProductFiles& files_;
        IssueCollector& issues_;
        DocumentChecks& documents_;
        const SourceContext& context_;
        std::string subject_;
        std::map<std::pair<uint16_t, EntryId>, DependencyOrigins> entryReferences_;
        size_t nextMenu_ = 0;
        size_t depth_ = 0;
    };
}

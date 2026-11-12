//
// kiwakiwaaにより 2026/10/19 に作成されました。
//

#pragma once

#include "MKD/dictionary/navigation.hpp"
#include "MKD/resource/appendix_entry_list.hpp"
#include "prepared_product_files.hpp"
#include "authoring_diagnostics.hpp"
#include "binary/binary_buffer.hpp"
#include "dictionary/navigation/index_format.hpp"
#include <set>
#include <map>
#include <pugixml.hpp>

namespace MKD::detail
{
    class DocumentChecks;
    class ProductBuild;

    class NavigationWriter
    {
    public:
        static void prepare(const ProductBuild& product, const NavigationIndex& index, const AppendixMenu& appendices,
                            PreparedProductFiles& files, IssueCollector& issues, DocumentChecks& documents, const SourceContext& context);

    private:
        struct Node
        {
            using Sections = std::vector<Node>;

            struct EntryTable
            {
                std::vector<NavigationItem> items;
                bool labelled = false;
            };

            using FileTable = std::vector<std::pair<std::string, std::string>>;
            NavigationLabel title = std::string{};
            uint16_t flags = 0;
            std::variant<Sections, EntryTable, FileTable, NavigationReference> content;
            [[nodiscard]] monokakido::navigation::ItemKind kind() const;
        };

        class Encoder
        {
        public:
            explicit Encoder(uint32_t kind);
            Result<void> node(const Node& node, size_t base);
            [[nodiscard]] std::span<const uint8_t> bytes() const { return bytes_; }

        private:
            uint32_t text(std::string_view text, size_t base);
            void patch(size_t offset, const BinaryBuffer& record);
            uint32_t append(const BinaryBuffer& record, size_t base);
            std::vector<uint8_t> bytes_;
        };

        explicit NavigationWriter(const ProductBuild& product, PreparedProductFiles& files, IssueCollector& issues, DocumentChecks& documents,
                                  const SourceContext& context);
        Result<std::string> index(const NavigationIndex& index, std::string title, size_t depth,
                                  std::optional<std::string> filename = std::nullopt);
        Result<Node> binary(const NavigationIndex& index, std::string title, size_t depth);
        Result<Node> section(const NavigationSection& section, size_t depth, NavigationKind kind,
                             std::string_view subject, size_t firstItem);
        Result<void> plist(const NavigationIndex& index, std::string_view title, std::string_view path, size_t depth);
        Result<void> plistItem(pugi::xml_node parent, const NavigationItem& item, size_t depth,
                               std::string_view subject);
        Result<std::string> targetFile(const NavigationItem& item, size_t depth, std::string title = {});
        AppendixEntryLink entry(AppendixEntryLink target, std::string subject);
        Result<void> label(const NavigationLabel& label, std::string_view subject);
        Result<void> save(std::string_view path, std::span<const uint8_t> data) const;
        Result<NavigationIndex> appendices(const AppendixMenu& menu, size_t depth);
        Result<AppendixEntryList> entryList(const AppendixList& list) const;
        Result<std::string> gallery(const AppendixGallery& gallery);
        std::string nextPath(std::string_view extension);
        std::string indexRelative(std::string_view path) const;
        const ProductBuild& product_;
        PreparedProductFiles& files_;
        IssueCollector& issues_;
        DocumentChecks& documents_;
        const SourceContext& context_;
        std::filesystem::path contents_;
        std::string directory_;
        bool kanwa_;
        size_t next_ = 0;
        std::map<std::pair<uint16_t, EntryId>, DependencyOrigins> entryReferences_;
        std::map<uint32_t, std::pair<std::string, std::string>> types_;
    };
}

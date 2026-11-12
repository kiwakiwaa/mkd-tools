//
// kiwakiwaaにより 2026/10/17 に作成されました。
//

#pragma once

#include "MKD/dictionary/product_writer.hpp"

namespace MKD::detail
{
    class DocumentChecks;
    class ProductBuild;
    class PreparedProductFiles;
    class IssueCollector;

    class ProductInformationWriter
    {
    public:
        static Result<void> validate(const ProductInformation& information);
        static void prepare(const ProductBuild& product, const DictionaryMetadata& metadata,
                            PreparedProductFiles& files, IssueCollector& issues, DocumentChecks& documents,
                            const SourceContext& context);
    };
}

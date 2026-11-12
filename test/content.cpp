//
// kiwakiwaaにより 2026/10/18 に作成されました。
//

#include "support/test_dictionary.hpp"
#include "dictionary/content/html_lexer.hpp"
#include <unordered_map>

namespace MKD::test
{
    using ContentTest = DictionaryTest;

    TEST(InlineContent, RubyGaijiAndLinksShareXmlAndHeadlineRepresentations)
    {
        InlineText label;
        label.append(Ruby{"鶫", "つぐみ"});
        label.append(" & ");
        label.append(Gaiji{{"gaiji/tsugumi.svg"}, "鶫"});
        label.append(EntryLink{{3, 0}, "セイバーライオン"});
        const auto headline = require(label.toHeadline());
        EXPECT_EQ(require(InlineText::fromHeadline(headline)).toHeadline().value(), headline);
        EXPECT_EQ(require(label.plainText()), "鶫 & 鶫セイバーライオン");
        const auto xml = require(label.toXhtml());
        EXPECT_NE(xml.find("<ruby>"), std::string::npos);
        EXPECT_NE(xml.find("&amp;"), std::string::npos);
        EXPECT_NE(xml.find("href=\"00003-0000\""), std::string::npos);
        EXPECT_NE(xml.find("gaiji/tsugumi.svg"), std::string::npos);
    }

    TEST(InlineContent, PreservesUnrecognisedHeadlineMarkupWithoutInventingXml)
    {
        const auto text = require(InlineText::fromHeadline(u"{B:がおー}"));
        EXPECT_EQ(require(text.toHeadline()), u"{B:がおー}");
        EXPECT_FALSE(text.toXhtml());
        EXPECT_FALSE(text.plainText());
        EXPECT_FALSE(InlineText::fromHeadline(u"{RB:broken"));
        InlineText unsafe;
        unsafe.append(Gaiji{{"../saberlion.png"}, "獅子"});
        EXPECT_FALSE(unsafe.toXhtml());
    }

    TEST_F(ContentTest, ResolvesLooseGaijiAndRegisteredStylesheets)
    {
        const auto gaiji = require(birbs().resource("gaiji/tsugumi.svg"));
        EXPECT_EQ(readFile(assets() / "gaiji.svg"), std::vector(gaiji.begin(), gaiji.end()));
        ASSERT_TRUE(birbs().stylesheet());
        ASSERT_TRUE(birbs().nightmodeStylesheet());
        EXPECT_EQ(birbs().presentation().fullRubyStylesheet, "birbs-ruby.css");
        EXPECT_NE(require(birbs().resource("birbs.css")).utf8().value().find("BirbMincho"), std::string::npos);
    }

    TEST(EntryIds, CanonicalTargetsWorkAsHashKeys)
    {
        const EntryId target{7, 0x8001};
        EXPECT_EQ(target.toString(), "00007-8001");
        std::unordered_map<EntryId, std::string> names{{target, "雀躍"}};
        EXPECT_EQ(names.at(target), "雀躍");
    }

    TEST(ContentAnalysis, RecognisesEscapedUrlsWithoutTreatingStringsAsDependencies)
    {
        const std::string css = R"css(p { content: "url(fake.png)"; background: u\72l("im\61 ge.png"); }
            /* url(comment.png) */ @import "other.css";
            @media print { p { background: url(print.png); } })css";
        auto analysis = require(content::analyzeCss(std::string(css)));
        ASSERT_EQ(analysis.references.size(), 3);
        EXPECT_EQ(std::get<content::AssetReference>(analysis.references[0].target).path, "image.png");
        EXPECT_EQ(std::get<content::AssetReference>(analysis.references[1].target).path, "other.css");
        EXPECT_EQ(std::get<content::AssetReference>(analysis.references[2].target).path, "print.png");
        ASSERT_TRUE(analysis.references[0].source);
        const auto source = *analysis.references[0].source;
        EXPECT_TRUE(std::string_view(css).substr(source.offset, source.length).starts_with("u\\72l("));
        ASSERT_EQ(analysis.diagnostics.size(), 2);
        EXPECT_EQ(analysis.diagnostics[0].kind, content::DiagnosticKind::IgnoredAtRule);

        auto article = require(content::analyzeArticle(7, std::string(
            "<html><body><p id=\"anchor\"/><a href=\"#anchor\"/>"
            "<img style=\"fill: url(#paint)\" src=\"../image.png\"/></body></html>")));
        EXPECT_TRUE(article.anchors.contains({7, 0}));
        ASSERT_EQ(article.references.size(), 3);
        EXPECT_EQ(std::get<content::AssetReference>(article.references.back().target).path, "../image.png");

        const auto keywords = require(content::analyzeCss(
            "@color accent = #000000; @COLOR accent = #000000; p { color: @media; }"));
        ASSERT_EQ(keywords.diagnostics.size(), 1);
        EXPECT_EQ(keywords.diagnostics.front().message, "The Dictionaries article renderer ignores @COLOR");

        const std::string nested = "鳥{B:前{REF:9-0000:後}}";
        const auto headline = require(content::analyzeHeadline(u"鳥{B:前{REF:9-0000:後}}"));
        ASSERT_EQ(headline.references.size(), 1);
        ASSERT_TRUE(headline.references.front().source);
        EXPECT_EQ(headline.references.front().source->offset, nested.find("{REF:"));
    }

    TEST(HtmlSyntax, AttributeTokensBorrowTheirOriginalSourceAndDecodeOnDemand)
    {
        const std::string input = "<IMG SRC='bird&#46;png?x=1&amp;y=2' disabled>";
        detail::html::Lexer lexer(input);
        auto element = lexer.next();
        ASSERT_TRUE(element);
        EXPECT_EQ(element->kind, detail::html::TokenKind::StartElement);
        EXPECT_TRUE(element->named("img"));
        auto attribute = lexer.next();
        ASSERT_TRUE(attribute);
        EXPECT_TRUE(attribute->named("src"));
        EXPECT_EQ(attribute->value.data(), input.data() + input.find("bird"));
        EXPECT_EQ(attribute->decodedValue().value(), "bird.png?x=1&y=2");
        EXPECT_EQ(input.substr(attribute->source.offset, attribute->source.length), "SRC='bird&#46;png?x=1&amp;y=2'");
        auto boolean = lexer.next();
        ASSERT_TRUE(boolean);
        EXPECT_TRUE(boolean->named("disabled"));
        EXPECT_TRUE(boolean->value.empty());
    }

    TEST(HtmlSyntax, AttributeCharacterReferencesPreserveLiteralTextAndRejectInvalidValues)
    {
        const std::pair<std::string_view, std::string_view> cases[]{
            {"a&b=1;c", "a&b=1;c"}, {"bird&amp;tree", "bird&tree"},
            {"&#40165;", "鳥"}, {"&#x9CE5;", "鳥"}, {"tail&", "tail&"}
        };
        for (const auto& [input, expected] : cases)
        {
            const detail::html::Token token{detail::html::TokenKind::Attribute, "src", input, {}};
            const auto decoded = token.decodedValue();
            ASSERT_TRUE(decoded) << input;
            EXPECT_EQ(*decoded, expected) << input;
        }
        for (const auto input : {"&#0;", "&#xD800;", "&#x110000;", "&#x;", "&unknown;"})
        {
            const detail::html::Token token{detail::html::TokenKind::Attribute, "src", input, {}};
            EXPECT_FALSE(token.decodedValue()) << input;
        }
    }

}

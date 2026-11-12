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

    TEST(HtmlSyntax, RawTextAndCommentsDoNotInventImageReferences)
    {
        const auto analysis = content::analyzeDocument(
            "<!-- <img src='comment.png'> --><SCRIPT>let s = '<img src=script.png>'; </scripture>still script</SCRIPT>"
            "<textarea><img src=textarea.png></textarea><img src=real.png>");
        ASSERT_TRUE(analysis);
        ASSERT_EQ(analysis->references.size(), 1);
        EXPECT_EQ(std::get<content::AssetReference>(analysis->references.front().target).path, "real.png");
        EXPECT_EQ(analysis->diagnostics.size(), 1);
    }

    TEST(HtmlSyntax, ReferenceCollectionSeparatesFragmentsStylesAndCoverage)
    {
        const auto analysis = content::analyzeDocument(
            "<a id='reading' name='reading' href='other.html#reading'>Read</a>"
            "<style>p{background:url(image.png)}</style>"
            "<p id='reading' style='background:url(inline.png)' srcset='large.png 2x'>Duplicate</p>");
        ASSERT_TRUE(analysis);
        EXPECT_EQ(analysis->identifiers.size(), 1);
        EXPECT_TRUE(analysis->identifiersComplete);
        ASSERT_EQ(analysis->references.size(), 3);
        const auto& link = std::get<content::DocumentReference>(analysis->references.front().target);
        EXPECT_EQ(link.path, "other.html");
        EXPECT_EQ(link.fragment, "reading");
        EXPECT_EQ(std::ranges::count(analysis->diagnostics, content::DiagnosticKind::DuplicateIdentifier,
                                    &content::Diagnostic::kind), 1);
    }

    TEST(HtmlSyntax, ReportsIncompleteIdentifierCoverage)
    {
        const auto entity = content::analyzeDocument("<p id='caf&eacute;'>A named entity</p>");
        ASSERT_TRUE(entity);
        EXPECT_FALSE(entity->identifiersComplete);
        EXPECT_FALSE(entity->diagnostics.empty());
        const auto malformed = content::analyzeDocument("<img src='unterminated");
        ASSERT_TRUE(malformed);
        EXPECT_FALSE(malformed->identifiersComplete);
        EXPECT_FALSE(malformed->diagnostics.empty());
    }

    TEST(ContentAnalysis, ArticlesAndLooseHtmlShareDocumentFragmentReferences)
    {
        const std::string html = "<html><body><p id=\"reading\"/><a href=\"#reading\"/>"
                                 "<a href=\"other.html#section\"/></body></html>";
        const auto article = require(content::analyzeArticle(7, html));
        const auto document = require(content::analyzeDocument(html));
        ASSERT_EQ(article.references.size(), 2);
        ASSERT_EQ(document.references.size(), 2);
        for (size_t i = 0; i < article.references.size(); ++i)
        {
            const auto* articleLink = std::get_if<content::DocumentReference>(&article.references[i].target);
            const auto* documentLink = std::get_if<content::DocumentReference>(&document.references[i].target);
            ASSERT_NE(articleLink, nullptr);
            ASSERT_NE(documentLink, nullptr);
            EXPECT_EQ(articleLink->path, documentLink->path);
            EXPECT_EQ(articleLink->fragment, documentLink->fragment);
            EXPECT_EQ(articleLink->syntax, documentLink->syntax);
        }
        EXPECT_TRUE(std::get<content::DocumentReference>(article.references.front().target).path.empty());
        EXPECT_FALSE(content::analyzeArticle(7, "<html><a href=\"#missing\"/></html>"));
        EXPECT_TRUE(content::analyzeArticle(7, "<html><a href=\"#\"/></html>"));
    }

    TEST(ContentAnalysis, CssPaintFragmentsUseTheSharedRepresentationWithoutDocumentIdChecks)
    {
        const auto css = require(content::analyzeCss("p { fill: url(#paint); }"));
        ASSERT_EQ(css.references.size(), 1);
        const auto* fragment = std::get_if<content::DocumentReference>(&css.references.front().target);
        ASSERT_NE(fragment, nullptr);
        EXPECT_TRUE(fragment->path.empty());
        EXPECT_EQ(fragment->fragment, "paint");
        EXPECT_EQ(fragment->syntax, content::ResourceSyntax::CssUrl);
        EXPECT_TRUE(content::analyzeArticle(7, "<html><p style=\"fill:url(#paint)\"/></html>"));
    }

    TEST(ContentAnalysis, EntryReferencesShareOneTypeWithOptionalNativeDictionaryIdentity)
    {
        const auto article = require(content::analyzeArticle(7,
            "<html><a href=\"00007-F001\"/><a href=\"00007-F001-0\"/>"
            "<a href=\"00007-F001-2000\"/></html>"));
        ASSERT_EQ(article.references.size(), 3);
        for (const auto& reference : article.references)
        {
            const auto* entry = std::get_if<content::EntryReference>(&reference.target);
            ASSERT_NE(entry, nullptr);
            EXPECT_EQ(entry->entryId, (EntryId{7, 0xF001}));
        }
        EXPECT_FALSE(std::get<content::EntryReference>(article.references[0].target).dictionaryId);
        EXPECT_EQ(std::get<content::EntryReference>(article.references[1].target).dictionaryId, 0);
        EXPECT_EQ(std::get<content::EntryReference>(article.references[2].target).dictionaryId, 2000);
        EXPECT_TRUE(content::analyzeArticle(7, "<html><a href=\"#00008-F001-2000\"/></html>"));
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

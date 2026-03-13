//
// kiwakiwaaにより 2026/03/11 に作成されました。
//

#include <gtest/gtest.h>
#include <filesystem>
#include <algorithm>

#include "../test_listener.hpp"
#include "MKD/resource/keystore.hpp"
#include "MKD/platform/macos/macos_dictionary_source.hpp"
#include "../../src/platform/macos/fs.hpp"
#include "../../src/resource/keystore/keystore_search.hpp"

class KeystoreSearchTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        const auto containerPath = MKD::macOS::getContainerPathByGroupIdentifier(MKD::MONOKAKIDO_GROUP_ID);
        const auto dictionariesPath = containerPath / MKD::DICTIONARIES_SUBPATH;

        keystorePath_ = dictionariesPath / "KJT" / "Contents" / "KJT" / "key" / "jyukugo.keystore";

        // Load the keystore
        auto keystoreResult = MKD::Keystore::open(keystorePath_, "");
        ASSERT_TRUE(keystoreResult.has_value())
            << "Failed to load keystore: " << keystoreResult.error();

        keystore_ = std::make_unique<MKD::Keystore>(std::move(keystoreResult.value()));

        MKD::test::verbosePrint("Loaded keystore from: {}\n", keystorePath_.string());
        MKD::test::verbosePrint("Index sizes - Prefix: {}, Suffix: {}\n",
                                keystore_->indexSize(MKD::KeystoreIndex::Prefix),
                                keystore_->indexSize(MKD::KeystoreIndex::Suffix));
    }

    std::filesystem::path keystorePath_;
    std::unique_ptr<MKD::Keystore> keystore_;
};

TEST_F(KeystoreSearchTest, PrefixSearchExistingTermReturnsResults)
{
    auto result = MKD::keystoreSearchResults(*keystore_, "愛情", MKD::SearchMode::Prefix);
    ASSERT_TRUE(result.has_value()) << "Search failed: " << result.error();

    EXPECT_FALSE(result->empty()) << "Should find results for '愛情'";
    MKD::test::verbosePrint("Found {} results for '愛情'\n", result->size());

    // all results start with the prefix
    for (const auto& entry : *result)
    {
        std::string_view key = entry.key;
        EXPECT_TRUE(key.substr(0, 6) == "愛情" || key.substr(0, 6) == "爱情")
            << "Key '" << key << "' doesn't start with expected prefix";
        MKD::test::verbosePrint("{}\n", key);
    }
}

TEST_F(KeystoreSearchTest, PrefixSearchNonExistentTermReturnsEmpty)
{
    auto result = MKD::keystoreSearchResults(*keystore_, "存在しない言葉", MKD::SearchMode::Prefix);
    ASSERT_TRUE(result.has_value()) << "Search failed: " << result.error();
    EXPECT_TRUE(result->empty()) << "Should find no results for non-existent term";
}

TEST_F(KeystoreSearchTest, PrefixSearchEmptyQueryReturnsError)
{
    auto result = MKD::keystoreSearchResults(*keystore_, "", MKD::SearchMode::Prefix);
    EXPECT_FALSE(result.has_value()) << "Empty query should return error";
}

TEST_F(KeystoreSearchTest, ExactSearchExistingTermReturnsSingleResult)
{
    auto result = MKD::keystoreSearchResults(*keystore_, "正真", MKD::SearchMode::Exact);
    ASSERT_TRUE(result.has_value()) << "Search failed: " << result.error();
    EXPECT_EQ(result->size(), 1) << "Should only one exact match for '正真'";

    if (!result->empty())
    {
        EXPECT_EQ(result->front().key, "正真") << "First result should match exactly";
    }
}

TEST_F(KeystoreSearchTest, ExactSearchCaseInsensitive_eturnsSameResults)
{
    // Test case insensitivity with ASCII
    auto upperResult = MKD::keystoreSearchResults(*keystore_, "ABC", MKD::SearchMode::Exact);
    auto lowerResult = MKD::keystoreSearchResults(*keystore_, "abc", MKD::SearchMode::Exact);

    ASSERT_TRUE(upperResult.has_value());
    ASSERT_TRUE(lowerResult.has_value());

    EXPECT_EQ(upperResult->size(), lowerResult->size()) << "Case-insensitive search should return same number of results";
}

TEST_F(KeystoreSearchTest, SuffixSearchExistingSuffixReturnsResults)
{
    auto result = MKD::keystoreSearchResults(*keystore_, "愛", MKD::SearchMode::Suffix);
    ASSERT_TRUE(result.has_value()) << "Suffix search failed: " << result.error();

    MKD::test::verbosePrint("Found {} results ending with '愛'\n", result->size());

    // all results end with the suffix
    for (const auto& entry : *result)
    {
        std::string_view key = entry.key;
        EXPECT_TRUE(key.size() >= 3 && key.substr(key.size() - 3) == "愛")
            << "Key '" << key << "' doesn't end with expected suffix";
        MKD::test::verbosePrint("{}\n", key);
    }
}

TEST_F(KeystoreSearchTest, SuffixSearchNonExistentSuffixReturnsEmpty)
{
    auto result = MKD::keystoreSearchResults(*keystore_, "xyzzy", MKD::SearchMode::Suffix);
    ASSERT_TRUE(result.has_value()) << "Suffix search failed: " << result.error();

    EXPECT_TRUE(result->empty()) << "Should find no results for non-existent suffix";
}

// Search range tests
TEST_F(KeystoreSearchTest, SearchRangePrefixReturnsValidIndices)
{
    auto range = MKD::keystoreSearch(*keystore_, "愛", MKD::SearchMode::Prefix);
    ASSERT_TRUE(range.has_value()) << "Range search failed: " << range.error();

    EXPECT_LT(range->begin, range->end) << "Range should be non-empty";
    EXPECT_LE(range->end, keystore_->indexSize(range->indexType))
        << "Range end should not exceed index size";

    MKD::test::verbosePrint("Prefix range for '愛': [{}, {})\n", range->begin, range->end);

    // Verify that entries in the range actually have the prefix
    for (size_t i = range->begin; i < range->end && i < range->begin + 10; ++i)
    {
        auto entry = keystore_->entryAt(range->indexType, i);
        ASSERT_TRUE(entry.has_value());
        EXPECT_TRUE(entry->key.substr(0, 3) == "愛") << "Entry at index " << i << " has key '" << entry->key << "' without prefix";
    }
}

TEST_F(KeystoreSearchTest, SearchRangeSuffixReturnsValidIndices)
{
    auto range = MKD::keystoreSearch(*keystore_, "愛", MKD::SearchMode::Suffix);
    ASSERT_TRUE(range.has_value()) << "Range search failed: " << range.error();

    EXPECT_LT(range->begin, range->end) << "Range should be non-empty";
    EXPECT_EQ(range->indexType, MKD::KeystoreIndex::Suffix) << "Suffix search should use suffix index";

    MKD::test::verbosePrint("Suffix range for '愛': [{}, {})\n", range->begin, range->end);
}

TEST_F(KeystoreSearchTest, SearchWithSpacesHandlesIgnorableCharacters)
{
    auto withoutSpace = MKD::keystoreSearchResults(*keystore_, "愛情", MKD::SearchMode::Prefix);
    auto withSpace = MKD::keystoreSearchResults(*keystore_, "愛 情", MKD::SearchMode::Prefix);

    ASSERT_TRUE(withoutSpace.has_value());
    ASSERT_TRUE(withSpace.has_value());

    EXPECT_EQ(withoutSpace->size(), withSpace->size()) << "Search should ignore spaces";
}

TEST_F(KeystoreSearchTest, SearchWithHyphensHandlesIgnorableCharacters)
{
    auto withoutHyphen = MKD::keystoreSearchResults(*keystore_, "愛情", MKD::SearchMode::Prefix);
    auto withHyphen = MKD::keystoreSearchResults(*keystore_, "愛-情", MKD::SearchMode::Prefix);

    ASSERT_TRUE(withoutHyphen.has_value());
    ASSERT_TRUE(withHyphen.has_value());

    EXPECT_EQ(withoutHyphen->size(), withHyphen->size())
        << "Search should ignore hyphens";
}

TEST_F(KeystoreSearchTest, UpperBoundWorksCorrectly)
{
    auto range = MKD::keystoreSearch(*keystore_, "愛", MKD::SearchMode::Prefix);
    ASSERT_TRUE(range.has_value());

    if (range->begin < range->end)
    {
        auto lastEntry = keystore_->entryAt(range->indexType, range->end - 1);
        ASSERT_TRUE(lastEntry.has_value());

        if (range->end < keystore_->indexSize(range->indexType))
        {
            auto nextEntry = keystore_->entryAt(range->indexType, range->end);
            ASSERT_TRUE(nextEntry.has_value());

            // The next entry should not have the prefix
            EXPECT_FALSE(nextEntry->key.substr(0, 3) == "愛") << "Entry after range should not have prefix";
        }
    }
}

TEST_F(KeystoreSearchTest, RetrievedEntriesHaveValidPageReferences)
{
    auto results = MKD::keystoreSearchResults(*keystore_, "愛情", MKD::SearchMode::Exact);
    ASSERT_TRUE(results.has_value());

    if (!results->empty())
    {
        const auto& entry = results->front();
        EXPECT_FALSE(entry.entryIds.empty()) << "Entry should have at least one page reference";

        for (const auto& [pageId, itemId] : entry.entryIds)
        {
            MKD::test::verbosePrint("Page: {}, Item: {}\n", pageId, itemId);
        }
    }
}

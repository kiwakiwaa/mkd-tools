//
// kiwakiwaaにより 2026/01/19 に作成されました。
//

#include <gtest/gtest.h>
#include <filesystem>

#include "../test_listener.hpp"
#include "MKD/platform/macos/fs.hpp"
#include "MKD/platform/macos/macos_dictionary_source.hpp"
#include "MKD/resource/rsc/rsc_index.hpp"
#include "MKD/resource/rsc/rsc_data.hpp"

class RscIndexTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        const auto containerPath = MKD::macOS::getContainerPathByGroupIdentifier(MKD::MONOKAKIDO_GROUP_ID);
        const auto dictionariesPath = containerPath / MKD::DICTIONARIES_SUBPATH;

        testDataPath_ = dictionariesPath / "YDP" / "Contents" / "YDP" / "contents";
    }

    std::filesystem::path testDataPath_;
};


TEST_F(RscIndexTest, LoadValidIndexFile)
{
    auto result = MKD::RscIndex::load(testDataPath_);
    ASSERT_TRUE(result.has_value()) << "Failed to load index: " << result.error();
    const auto& index = result.value();
    const size_t recordCount = index.size();

    MKD::test::verbosePrint("RSC Index Loaded Successfully from: {}\n", testDataPath_.string());
    MKD::test::verbosePrint("Total Records: {}\n", recordCount);

    EXPECT_GT(recordCount, 0) << "Index should contain at least one record";
    EXPECT_FALSE(index.empty()) << "Index should not be empty";
}

TEST_F(RscIndexTest, GetRecordByIndex)
{
    const auto indexResult = MKD::RscIndex::load(testDataPath_);
    ASSERT_TRUE(indexResult.has_value());

    const auto& index = indexResult.value();

    MKD::test::verbosePrint("First 10 Records:\n");

    const size_t displayCount = std::min(index.size(), size_t{10});

    for (size_t i = 0; i < displayCount; ++i)
    {
        auto recordResult = index.getByIndex(i);
        ASSERT_TRUE(recordResult.has_value()) << "Failed to get record at index " << i;

        const auto& [itemId, mapRecord] = recordResult.value();
        MKD::test::verbosePrint("  [{:4}] ID: {:8} | {}\n", i, itemId, mapRecord);

        // Validate record fields are reasonable
        EXPECT_GE(mapRecord.chunkGlobalOffset, 0u) << "chunkGlobalOffset should be non-negative";
        EXPECT_GE(mapRecord.itemOffset, 0u) << "itemOffset should be non-negative";
    }
}

TEST_F(RscIndexTest, GetOutOfBoundsIndex)
{
    const auto indexResult = MKD::RscIndex::load(testDataPath_);
    ASSERT_TRUE(indexResult.has_value());

    const auto& index = indexResult.value();
    const size_t invalidIndex = index.size() + 100;

    auto result = index.getByIndex(invalidIndex);

    ASSERT_FALSE(result.has_value()) << "Should fail for out of bounds index";

    MKD::test::verbosePrint("Expected error (index {} out of range): {}\n", invalidIndex, result.error());

    EXPECT_TRUE(result.error().find("out of range") != std::string::npos ||
        result.error().find("invalid index") != std::string::npos);
}

TEST_F(RscIndexTest, FindRecordById)
{
    auto indexResult = MKD::RscIndex::load(testDataPath_);
    ASSERT_TRUE(indexResult.has_value());

    const auto& index = indexResult.value();

    // Get the 10th record's ID to test with
    auto recordResult = index.getByIndex(9);
    ASSERT_TRUE(recordResult.has_value());

    const auto& [testItemId, expectedRecord] = recordResult.value();

    MKD::test::verbosePrint("Testing findById with itemId: {}\n", testItemId);

    auto findResult = index.findById(testItemId);
    ASSERT_TRUE(findResult.has_value()) << "Failed to find record by ID: " << testItemId;

    const auto& foundRecord = findResult.value();

    MKD::test::verbosePrint("Found record:\n");
    MKD::test::verbosePrint("  [9] ID: {:8} | {}\n", testItemId, foundRecord);

    // Verify the found record matches
    EXPECT_EQ(foundRecord.chunkGlobalOffset, expectedRecord.chunkGlobalOffset);
    EXPECT_EQ(foundRecord.itemOffset, expectedRecord.itemOffset);
}



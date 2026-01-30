//
// kiwakiwaaにより 2026/01/19 に作成されました。
//

#include <gtest/gtest.h>
#include <filesystem>

#include "monokakido/core/platform/fs.hpp"
#include "monokakido/dictionary/catalog.hpp"
#include "monokakido/resource/rsc/rsc_index.hpp"
#include "monokakido/resource/rsc/rsc_data.hpp"
#include "../common.hpp"

using namespace monokakido::resource;
using namespace monokakido::test;

class RscIndexTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        const auto containerPath = monokakido::platform::fs::getContainerPathByGroupIdentifier(
            monokakido::dictionary::MONOKAKIDO_GROUP_ID);
        const auto dictionariesPath = containerPath / monokakido::dictionary::DICTIONARIES_PATH;

        testDataPath_ = dictionariesPath / "YDP" / "Contents" / "YDP" / "contents";
    }

    std::filesystem::path testDataPath_;
};


void printMapRecord(uint32_t itemId, const MapRecord& record, size_t index = 0)
{
    std::cout << std::format("  [{:4}] ID: {:8} | zOffset: {:10} | ioffset: {:6}\n",
                             index,
                             itemId,
                             record.zOffset,
                             record.ioffset
    );
}


TEST_F(RscIndexTest, LoadValidIndexFile)
{
    auto result = RscIndex::load(testDataPath_);
    ASSERT_TRUE(result.has_value()) << "Failed to load index: " << result.error();
    const auto& index = result.value();
    const size_t recordCount = index.size();

    std::cout << "\n";
    printSeparator();
    std::cout << std::format("RSC Index Loaded Successfully from: {}\n", testDataPath_.string());
    printSeparator();
    std::cout << std::format("Total Records: {}\n", recordCount);
    printSeparator();
    std::cout << "\n";

    EXPECT_GT(recordCount, 0) << "Index should contain at least one record";
    EXPECT_FALSE(index.empty()) << "Index should not be empty";
}

TEST_F(RscIndexTest, GetRecordByIndex)
{
    const auto indexResult = RscIndex::load(testDataPath_);
    ASSERT_TRUE(indexResult.has_value());

    const auto& index = indexResult.value();

    std::cout << "\n";
    printSeparator();
    std::cout << "First 10 Records:\n";
    printSeparator();

    const size_t displayCount = std::min(index.size(), size_t{10});

    for (size_t i = 0; i < displayCount; ++i)
    {
        auto recordResult = index.getByIndex(i);
        ASSERT_TRUE(recordResult.has_value()) << "Failed to get record at index " << i;

        const auto& [itemId, mapRecord] = recordResult.value();
        printMapRecord(itemId, mapRecord, i);

        // Validate record fields are reasonable
        EXPECT_GE(mapRecord.zOffset, 0u) << "zOffset should be non-negative";
        EXPECT_GE(mapRecord.ioffset, 0u) << "ioffset should be non-negative";
    }

    std::cout << "\n";
}

TEST_F(RscIndexTest, GetOutOfBoundsIndex)
{
    const auto indexResult = RscIndex::load(testDataPath_);
    ASSERT_TRUE(indexResult.has_value());

    const auto& index = indexResult.value();
    const size_t invalidIndex = index.size() + 100;

    auto result = index.getByIndex(invalidIndex);

    ASSERT_FALSE(result.has_value()) << "Should fail for out of bounds index";

    std::cout << "\n";
    std::cout << std::format("Expected error (index {} out of range): {}\n\n", invalidIndex, result.error());

    EXPECT_TRUE(result.error().find("out of range") != std::string::npos ||
        result.error().find("invalid index") != std::string::npos);
}

TEST_F(RscIndexTest, FindRecordById)
{
    auto indexResult = RscIndex::load(testDataPath_);
    ASSERT_TRUE(indexResult.has_value());

    const auto& index = indexResult.value();

    // Get the 10th record's ID to test with
    auto recordResult = index.getByIndex(9);
    ASSERT_TRUE(recordResult.has_value());

    const auto& [testItemId, expectedRecord] = recordResult.value();

    std::cout << "\n";
    printSeparator();
    std::cout << std::format("Testing findById with itemId: {}\n", testItemId);
    printSeparator();

    auto findResult = index.findById(testItemId);
    ASSERT_TRUE(findResult.has_value()) << "Failed to find record by ID: " << testItemId;

    const auto& foundRecord = findResult.value();

    std::cout << "Found record:\n";
    printMapRecord(testItemId, foundRecord);
    std::cout << "\n";

    // Verify the found record matches
    EXPECT_EQ(foundRecord.zOffset, expectedRecord.zOffset);
    EXPECT_EQ(foundRecord.ioffset, expectedRecord.ioffset);
}



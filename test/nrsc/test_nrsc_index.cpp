//
// kiwakiwaaにより 2026/01/16 に作成されました。
//

#include <gtest/gtest.h>
#include <filesystem>

#include "monokakido/core/platform/fs.hpp"
#include "monokakido/dictionary/catalog.hpp"
#include "monokakido/resource/nrsc/nrsc_index.hpp"
#include "../test_listener.hpp"

using namespace monokakido;

class NrscIndexTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        const auto containerPath = platform::fs::getContainerPathByGroupIdentifier(MONOKAKIDO_GROUP_ID);
        const auto dictionariesPath = containerPath / DICTIONARIES_PATH;

        testDataPath_ = dictionariesPath / "KJT" / "Contents" / "KJT" / "img";
    }

    std::filesystem::path testDataPath_;
};


TEST_F(NrscIndexTest, LoadValidIndexFile)
{
    auto result = NrscIndex::load(testDataPath_);
    ASSERT_TRUE(result.has_value()) << "Failed to load index: " << result.error();

    const auto& index = result.value();
    const size_t recordCount = index.size();

    test::verbosePrint("NRSC Index Loaded Successfully from: {}\n", testDataPath_.string());
    test::verbosePrint("Total Records: {}\n", recordCount);

    EXPECT_GT(recordCount, 0) << "Index should contain at least one record";
}


TEST_F(NrscIndexTest, LoadNonExistentDirectory)
{
    auto result = NrscIndex::load("/path/that/does/not/exist");

    ASSERT_FALSE(result.has_value()) << "Should fail when directory doesn't exist";

    test::verbosePrint("Expected error (non-existent directory): {}\n", result.error());

    EXPECT_TRUE(result.error().find("not found") != std::string::npos);
}


TEST_F(NrscIndexTest, GetRecordByIndex)
{
    auto indexResult = NrscIndex::load(testDataPath_);
    ASSERT_TRUE(indexResult.has_value());

    const auto& index = indexResult.value();

    test::verbosePrint("First 10 Records:\n");

    const size_t displayCount = std::min(index.size(), size_t{10});

    for (size_t i = 0; i < displayCount; ++i)
    {
        auto recordResult = index.getByIndex(i);
        ASSERT_TRUE(recordResult.has_value()) << "Failed to get record at index " << i;

        const auto& [id, record] = recordResult.value();
        test::verbosePrint("  [{:4}] ID: {:20} | {}\n", i, id, record);

        // Validate record fields
        EXPECT_LE(record.compressionFormat(), CompressionFormat::Zlib);
    }
}


TEST_F(NrscIndexTest, GetOutOfBoundsIndex)
{
    auto indexResult = NrscIndex::load(testDataPath_);
    ASSERT_TRUE(indexResult.has_value());

    const auto& index = indexResult.value();
    const size_t invalidIndex = index.size() + 100;

    auto result = index.getByIndex(invalidIndex);

    ASSERT_FALSE(result.has_value()) << "Should fail for out of bounds index";

    test::verbosePrint("Expected error (index {} out of range): {}", invalidIndex, result.error());

    EXPECT_TRUE(result.error().find("out of range") != std::string::npos);
}


TEST_F(NrscIndexTest, FindRecordById)
{
    auto indexResult = NrscIndex::load(testDataPath_);
    ASSERT_TRUE(indexResult.has_value());

    const auto& index = indexResult.value();

    // Get the 20th record's ID to test with
    auto firstRecordResult = index.getByIndex(20);
    ASSERT_TRUE(firstRecordResult.has_value());

    const auto& [testId, expectedRecord] = firstRecordResult.value();

    test::verbosePrint("Testing findById with ID: '{}'\n", testId);

    auto findResult = index.findById(testId);
    ASSERT_TRUE(findResult.has_value()) << "Failed to find record by ID: " << testId;

    const auto& foundRecord = findResult.value();

    test::verbosePrint("Found record:\n");
    test::verbosePrint("  [20] ID: {:20} | {}\n", testId, foundRecord);

    // Verify the found record matches
    EXPECT_EQ(foundRecord.fileSeq(), expectedRecord.fileSeq());
    EXPECT_EQ(foundRecord.offset(), expectedRecord.offset());
    EXPECT_EQ(foundRecord.len(), expectedRecord.len());
    EXPECT_EQ(foundRecord.compressionFormat(), expectedRecord.compressionFormat());
}


TEST_F(NrscIndexTest, DisplayIndexStatistics)
{
    const auto indexResult = NrscIndex::load(testDataPath_);
    ASSERT_TRUE(indexResult.has_value());

    const auto& index = indexResult.value();

    size_t uncompressedCount = 0;
    size_t zlibCount = 0;
    size_t totalSize = 0;
    size_t maxLength = 0;
    size_t minLength = std::numeric_limits<size_t>::max();

    std::map<uint16_t, size_t> fileDistribution;

    for (const auto record : index | std::views::values)
    {
        if (record.compressionFormat() == CompressionFormat::Uncompressed)
            ++uncompressedCount;
        else
            ++zlibCount;

        totalSize += record.len();
        maxLength = std::max(maxLength, record.len());
        minLength = std::min(minLength, record.len());

        ++fileDistribution[record.fileSeq()];
    }

    test::verbosePrint("  Total Records:        {}\n", index.size());
    test::verbosePrint(
        "  Uncompressed:         {} ({:.1f}%)\n",
        uncompressedCount,
        100.0 * static_cast<double>(uncompressedCount) / static_cast<double>(index.size())
    );
    test::verbosePrint(
        "  Zlib Compressed:      {} ({:.1f}%)\n",
        zlibCount,
        100.0 * static_cast<double>(zlibCount) / static_cast<double>(index.size())
    );
    test::verbosePrint("  Total Size:           {} bytes\n", totalSize);
    test::verbosePrint("  Average Record Size:  {} bytes\n", totalSize / index.size());
    test::verbosePrint("  Min Record Size:      {} bytes\n", minLength);
    test::verbosePrint("  Max Record Size:      {} bytes\n", maxLength);
    test::verbosePrint("\n  Distribution across .nrsc files:\n");

    for (const auto& [fileSeq, count] : fileDistribution)
    {
        test::verbosePrint("    {}.nrsc: {} records ({:.1f}%)\n",
                                 fileSeq,
                                 count,
                                 100.0 * static_cast<double>(count) / static_cast<double>(index.size())
        );
    }

    EXPECT_GT(index.size(), 0);
    EXPECT_GT(totalSize, 0);
}


TEST_F(NrscIndexTest, DisplayLastRecords)
{
    auto indexResult = NrscIndex::load(testDataPath_);
    ASSERT_TRUE(indexResult.has_value());

    const auto& index = indexResult.value();

    test::verbosePrint("Last 5 Records:\n");

    const size_t displayCount = std::min(index.size(), size_t{5});
    const size_t startIndex = index.size() - displayCount;

    for (size_t i = startIndex; i < index.size(); ++i)
    {
        auto recordResult = index.getByIndex(i);
        ASSERT_TRUE(recordResult.has_value());

        const auto& [id, record] = recordResult.value();
        test::verbosePrint("  [{:4}] ID: {:20} | {}\n", i, id, record);
    }
}

//
// kiwakiwaaにより 2026/01/29 に作成されました。
//

#include <gtest/gtest.h>
#include <filesystem>
#include <algorithm>

#include "monokakido/core/platform/fs.hpp"
#include "monokakido/dictionary/catalog.hpp"
#include "monokakido/resource/rsc/rsc_index.hpp"
#include "monokakido/resource/rsc/rsc_data.hpp"
#include "monokakido/resource/xml_view.hpp"
#include "../common.hpp"

using namespace monokakido::resource;
using namespace monokakido::test;

class RscDataTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        const auto containerPath = monokakido::platform::fs::getContainerPathByGroupIdentifier(
            monokakido::dictionary::MONOKAKIDO_GROUP_ID);
        const auto dictionariesPath = containerPath / monokakido::dictionary::DICTIONARIES_PATH;

        testDataPath_ = dictionariesPath / "YDP" / "Contents" / "YDP" / "contents";
        dictId_ = "YDP";
    }

    std::filesystem::path testDataPath_;
    std::string dictId_;
};


void printResourceFileInfo(const RscResourceFile& file, size_t index)
{
    std::cout << std::format("  [{:2}] File: {}.rsc | Offset: {:10} | Length: {:10} | Path: {}\n",
                             index,
                             file.sequenceNumber,
                             file.globalOffset,
                             file.length,
                             file.filePath.filename().string());
}

TEST_F(RscDataTest, LoadValidRscData)
{
    auto result = RscData::load(testDataPath_, dictId_);
    ASSERT_TRUE(result.has_value()) << "Failed to load RSC data: " << result.error();

    std::cout << "\n";
    printSeparator();
    std::cout << std::format("RSC Data Loaded Successfully from: {}\n", testDataPath_.string());
    std::cout << std::format("Dictionary ID: {}\n", dictId_);
    printSeparator();
    std::cout << "\n";
}


TEST_F(RscDataTest, GetRecordData)
{
    auto dataResult = RscData::load(testDataPath_, dictId_);
    ASSERT_TRUE(dataResult.has_value());

    auto indexResult = RscIndex::load(testDataPath_);
    ASSERT_TRUE(indexResult.has_value());

    auto& rscData = dataResult.value();
    const auto& index = indexResult.value();

    std::cout << "\n";
    printSeparator();
    std::cout << "Testing Record Data Retrieval:\n";
    printSeparator();

    // Get first record
    auto recordResult = index.getByIndex(0);
    ASSERT_TRUE(recordResult.has_value());

    const auto& [itemId, mapRecord] = recordResult.value();

    auto dataSpan = rscData.get(mapRecord);
    ASSERT_TRUE(dataSpan.has_value()) << "Failed to get data: " << dataSpan.error();

    const auto& data = dataSpan.value();

    const auto xmlResult = XmlView{data}.asStringView();
    ASSERT_TRUE(xmlResult.has_value()) << "Data contains invalid UTF-8 sequence: " << xmlResult.error();

    const auto& xmlView = xmlResult.value();

    std::cout << std::format("  Item ID:     {}\n", itemId);
    std::cout << std::format("  Data Size:   {} bytes\n", data.size());
    std::cout << std::format("  First 100 chars: {}\n", xmlView.substr(0, 100));

    printSeparator();
    std::cout << "\n";

    EXPECT_GT(data.size(), 0) << "Retrieved data should not be empty";
}



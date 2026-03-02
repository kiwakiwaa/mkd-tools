//
// kiwakiwaaにより 2026/01/29 に作成されました。
//

#include <gtest/gtest.h>
#include <filesystem>
#include <algorithm>

#include "../test_listener.hpp"
#include "MKD/platform/macos/fs.hpp"
#include "MKD/platform/macos/macos_dictionary_source.hpp"
#include "MKD/resource/rsc/rsc.hpp"
#include "MKD/resource/xml_view.hpp"

#include <pugixml.h>

class RscDataTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        const auto containerPath = MKD::macOS::getContainerPathByGroupIdentifier(MKD::MONOKAKIDO_GROUP_ID);
        const auto dictionariesPath = containerPath / MKD::DICTIONARIES_SUBPATH;

        dictId_ = "KJT.J";
        testDataPath_ = dictionariesPath / "KJT" / "Contents" / "KJT" / "contents";

        testFontDataPath_ = dictionariesPath / "KOGO3" / "Contents" / "ozk5" / "fonts" / "YuMinPr6N-R";
        testAudioDataPath_ = dictionariesPath / "GrossesDe2" / "Contents" / "GDJ2" / "audio";
    }

    std::filesystem::path testDataPath_;
    std::filesystem::path testFontDataPath_;
    std::filesystem::path testAudioDataPath_;
    std::string dictId_;
};


TEST_F(RscDataTest, LoadValidRscData)
{
    auto result = MKD::RscData::load(testDataPath_, dictId_);
    ASSERT_TRUE(result.has_value()) << "Failed to load RSC data: " << result.error();

    MKD::test::verbosePrint("RSC Data Loaded Successfully from: {}\n", testDataPath_.string());
    MKD::test::verbosePrint("Dictionary ID: {}\n", dictId_);
}


TEST_F(RscDataTest, GetRecordData)
{
    auto indexResult = MKD::RscIndex::load(testDataPath_);
    ASSERT_TRUE(indexResult.has_value());

    auto dataResult = MKD::RscData::load(testDataPath_, dictId_, indexResult->mapVersion());
    ASSERT_TRUE(dataResult.has_value());

    auto& rscData = dataResult.value();
    const auto& index = indexResult.value();

    // Get first record
    auto recordResult = index.getByIndex(0);
    ASSERT_TRUE(recordResult.has_value());

    const auto& [itemId, mapRecord] = recordResult.value();

    auto dataSpan = rscData.get(mapRecord);
    ASSERT_TRUE(dataSpan.has_value()) << "Failed to get data: " << dataSpan.error();

    const auto& data = dataSpan.value();
    EXPECT_GT(data.size(), 0) << "Retrieved data should not be empty";

    const auto xmlResult = MKD::XmlView{data.span()}.asStringView();
    ASSERT_TRUE(xmlResult.has_value()) << "Data contains invalid UTF-8 sequence: " << xmlResult.error();

    MKD::test::verbosePrint("Beginning of xml: {}\n", xmlResult.value().substr(0, 50));

    pugi::xml_document xmlDoc;
    auto parseResult = xmlDoc.load_buffer(xmlResult.value().data(), xmlResult.value().size());
    ASSERT_TRUE(parseResult.status == pugi::status_ok) << std::format("Data contains invalid XML: {}",
                                                                      parseResult.description());
}

TEST_F(RscDataTest, GetAudioData)
{
    auto indexResult = MKD::RscIndex::load(testAudioDataPath_);
    ASSERT_TRUE(indexResult.has_value());

    MKD::test::verbosePrint("Amount of audio records: {}\n", indexResult->size());

    auto dataResult = MKD::RscData::load(testAudioDataPath_);
    ASSERT_TRUE(dataResult.has_value());

    auto& rscData = dataResult.value();
    const auto& index = indexResult.value();

    auto recordResult = index.getByIndex(0);
    ASSERT_TRUE(recordResult.has_value());

    const auto& [itemId, mapRecord] = recordResult.value();
    MKD::test::verbosePrint("  [0] ID: {:8} | {}\n", itemId, mapRecord);

    auto dataSpan = rscData.get(mapRecord);
    ASSERT_TRUE(dataSpan.has_value()) << "Failed to get data: " << dataSpan.error();
}

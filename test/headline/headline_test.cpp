//
// kiwakiwaaにより 2026/02/12 に作成されました。
//

#include <gtest/gtest.h>
#include <filesystem>

#include "../test_listener.hpp"
#include "MKD/platform/macos/fs.hpp"
#include "MKD/platform/macos/macos_dictionary_source.hpp"
#include "MKD/resource/headline/headline_store.hpp"

using namespace MKD;

class HeadlineTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        const auto containerPath = macOS::getContainerPathByGroupIdentifier(MONOKAKIDO_GROUP_ID);
        const auto dictionariesPath = containerPath / DICTIONARIES_SUBPATH;

        testHeadline_ = dictionariesPath / "KankenKJ2" / "Contents" / "KankenKJ2Data" / "headline" / "headline.headlinestore";
    }

    std::filesystem::path testHeadline_;
};


TEST_F(HeadlineTest, LoadValidHeadlineStore)
{
    auto headlineStoreResult = HeadlineStore::load(testHeadline_);
    ASSERT_TRUE(headlineStoreResult.has_value()) << "Failed to load headline store: " << headlineStoreResult.error();

    const auto& headlineStore = headlineStoreResult.value();

    test::verbosePrint("HeadlineStore Total Records: {}\n", headlineStore.size());
}
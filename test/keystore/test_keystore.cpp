//
// kiwakiwaaにより 2026/02/07 に作成されました。
//

#include <gtest/gtest.h>
#include <filesystem>

#include "../test_listener.hpp"
#include "MKD/platform/macos/fs.hpp"
#include "MKD/platform/macos/macos_dictionary_source.hpp"
#include "MKD/resource/keystore/keystore.hpp"

class KeystoreTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        const auto containerPath = MKD::macOS::getContainerPathByGroupIdentifier(MKD::MONOKAKIDO_GROUP_ID);
        const auto dictionariesPath = containerPath / MKD::DICTIONARIES_SUBPATH;

        testHeadwords_ = dictionariesPath / "KNEJ" / "Contents" / "KNEJ" / "key" / "headword.keystore";
    }

    std::filesystem::path testHeadwords_;
};


TEST_F(KeystoreTest, LoadValidKeystoreFile)
{
    auto result2 = MKD::Keystore::load(testHeadwords_, "KNEJ");
    ASSERT_TRUE(result2.has_value()) << "Failed to load keystore: " << result2.error();

    auto& keystore = result2.value();

    for (auto i = 0; i < keystore.indexSize(MKD::KeystoreIndex::Prefix); ++i)
    {
        auto lookupResult = keystore.getByIndex(MKD::KeystoreIndex::Prefix, i);
        ASSERT_TRUE(lookupResult.has_value()) << "Failed to get lookup: " << lookupResult.error();
    }
}

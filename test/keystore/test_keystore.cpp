//
// kiwakiwaaにより 2026/02/07 に作成されました。
//

#include <gtest/gtest.h>
#include <filesystem>

#include "monokakido/dictionary/catalog.hpp"
#include "monokakido/resource/keystore/keystore.hpp"
#include "../test_listener.hpp"

using namespace monokakido;

class KeystoreTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        const auto containerPath = platform::fs::getContainerPathByGroupIdentifier(MONOKAKIDO_GROUP_ID);
        const auto dictionariesPath = containerPath / DICTIONARIES_PATH;

        testHeadwords_ = dictionariesPath / "KNEJ" / "Contents" / "KNEJ" / "key" / "headword.keystore";
    }

    std::filesystem::path testHeadwords_;
};


TEST_F(KeystoreTest, LoadValidKeystoreFile)
{
    auto result2 = Keystore::load(testHeadwords_, "KNEJ");
    ASSERT_TRUE(result2.has_value()) << "Failed to load keystore: " << result2.error();

    auto& keystore = result2.value();

    for (auto i = 0; i < keystore.indexSize(KeystoreIndex::Prefix); ++i)
    {
        auto lookupResult = keystore.getByIndex(KeystoreIndex::Prefix, i);
        ASSERT_TRUE(lookupResult.has_value()) << "Failed to get lookup: " << lookupResult.error();
    }
}

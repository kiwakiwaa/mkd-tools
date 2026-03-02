//
// kiwakiwaaにより 2026/03/02 に作成されました。
//

#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>

#include "MKD/resource/rsc/rsc_crypto.hpp"
#include "../test_listener.hpp"

using namespace MKD;

TEST(RscCrypto, TestRoundTrip)
{
    const std::string dictId = "Test.dict";

    const std::filesystem::path xmlPath = std::filesystem::path(__FILE__).parent_path() / "test_entry.xml";
    test::verbosePrint("Reading XML from: {}\n", xmlPath.string());

    std::ifstream xmlFile(xmlPath);
    ASSERT_TRUE(xmlFile.is_open()) << "Failed to open test_entry.xml at: " << xmlPath.string();

    std::ostringstream ss;
    ss << xmlFile.rdbuf();
    const std::string plaintext = ss.str();

    ASSERT_FALSE(plaintext.empty()) << "test_entry.xml is empty";

    test::verbosePrint("Dictionary ID: {}\n", dictId);
    test::verbosePrint("Plaintext length: {} bytes\n", plaintext.size());
    test::verbosePrint("Plaintext content: \"{}\"\n", plaintext);

    auto key = RscCrypto::deriveKey(dictId);

    std::cout << "Derived key: ";
    for (unsigned char & i : key)
    {
        test::verbosePrint("{:02x} ", i);
    }
    test::verbosePrint("\n");

    std::span plaintextBytes(
        reinterpret_cast<const uint8_t*>(plaintext.data()),
        plaintext.size()
    );

    auto encrypted = RscCrypto::encrypt(plaintextBytes, key);
    test::verbosePrint("Encrypted data size: {} bytes\n", encrypted.size());

    std::cout << "Encrypted hex (" << encrypted.size() << " bytes):\n";
    for (size_t i = 0; i < encrypted.size(); ++i)
    {
        std::cout << std::format("{:02x}", encrypted[i]);
        if ((i + 1) % 16 == 0)
            std::cout << "\n";
        else
            std::cout << " ";
    }
    if (encrypted.size() % 16 != 0)
        std::cout << "\n";

    auto decryptedResult = RscCrypto::decrypt(encrypted, key);
    ASSERT_TRUE(decryptedResult.has_value()) << "Decryption failed: " << decryptedResult.error();

    const auto& decrypted = decryptedResult.value();
    std::string_view decryptedText(
        reinterpret_cast<const char*>(decrypted.data()),
        decrypted.size()
    );

    test::verbosePrint("Decrypted data size: {} bytes\n", decrypted.size());
    test::verbosePrint("Decrypted content: \"{}\"\n", decryptedText);

    ASSERT_EQ(plaintext.size(), decrypted.size()) << "Size mismatch after round trip";
    ASSERT_EQ(plaintext, decryptedText) << "Content mismatch after round trip";

    for (size_t i = 0; i < plaintext.size(); ++i)
    {
        ASSERT_EQ(static_cast<uint8_t>(plaintext[i]), decrypted[i]) << "Byte mismatch at position " << i;
    }
}

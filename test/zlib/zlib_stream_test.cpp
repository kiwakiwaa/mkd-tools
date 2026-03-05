//
// kiwakiwaaにより 2026/03/05 に作成されました。
//

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>

#include "../../src/resource/zlib_stream.hpp"
#include "../test_listener.hpp"

using namespace MKD;

TEST(ZlibStreamTest, CompressDecompressRoundTripWithText) {
    constexpr std::string_view text {"`祇園精舎の鐘の声諸行無常の響あり `娑羅双樹の花の色盛者必衰の理を顕す `奢れる人も久しからずただ春の夜の夢の如し `猛き者もつひには滅びぬ偏に風の前の塵に同じ"};

    std::span originalData(
        reinterpret_cast<const uint8_t*>(text.data()),
        text.size()
    );

    test::verbosePrint("Original data size: {} bytes\n", originalData.size());

    const ZlibStream compressor;
    auto compressResult = compressor.compress(originalData, Z_DEFAULT_COMPRESSION);
    ASSERT_TRUE(compressResult.has_value()) << "Compression failed: " << compressResult.error();

    const auto compressed = compressResult.value();
    test::verbosePrint("Compressed size: {} bytes (ratio: {:.2f}%)\n", compressed.size(), (100.0 * compressed.size() / originalData.size()));

    EXPECT_TRUE(ZlibStream::isZlibCompressed(compressed)) << "Compressed data not detected as zlib format";

    const ZlibStream decompressor;
    auto decompressResult = decompressor.decompress(compressed, originalData.size());
    ASSERT_TRUE(decompressResult.has_value()) << "Decompression failed: " << decompressResult.error();

    auto decompressed = decompressResult.value();
    test::verbosePrint("Decompressed size: {} bytes\n", decompressed.size());

    ASSERT_EQ(originalData.size(), decompressed.size()) << "Size mismatch after round trip";

    std::string_view decompressedText(
        reinterpret_cast<const char*>(decompressed.data()),
        decompressed.size()
    );
    EXPECT_EQ(text, decompressedText) << "Content mismatch after round trip";

    for (size_t i = 0; i < originalData.size(); ++i)
        ASSERT_EQ(originalData[i], decompressed[i]) << "Byte mismatch at position " << i;
}

TEST(ZlibStreamTest, CompressionLevels) {
    const std::filesystem::path xmlPath = std::filesystem::path(__FILE__).parent_path().parent_path() / "test_entry.xml";
    test::verbosePrint("Reading XML from: {}\n", xmlPath.string());

    std::ifstream xmlFile(xmlPath);
    ASSERT_TRUE(xmlFile.is_open()) << "Failed to open test_entry.xml at: " << xmlPath.string();

    std::ostringstream ss;
    ss << xmlFile.rdbuf();
    const std::string text = ss.str();

    std::span data(
        reinterpret_cast<const uint8_t*>(text.data()),
        text.size()
    );

    test::verbosePrint("Original data size: {} bytes\n", data.size());

    ZlibStream zlib;
    size_t bestCompression = data.size();
    size_t fastestCompression = data.size();
    size_t noCompression = data.size();

    // Test level 0
    {
        auto result = zlib.compress(data, 0);
        ASSERT_TRUE(result.has_value());
        noCompression = result.value().size();
        test::verbosePrint("Level 0 (no compression): {} bytes\n", noCompression);
    }

    // Test level 1
    {
        auto result = zlib.compress(data, 1);
        ASSERT_TRUE(result.has_value());
        fastestCompression = result.value().size();
        test::verbosePrint("Level 1 (fastest): {} bytes (ratio: {:.2f}%)\n", fastestCompression, 100.0 * fastestCompression / data.size());
    }

    // Test default level
    {
        auto result = zlib.compress(data, Z_DEFAULT_COMPRESSION);
        ASSERT_TRUE(result.has_value());
        size_t level6 = result.value().size();
        test::verbosePrint("Default level: {} bytes (ratio: {:.2f}%)\n", level6, 100.0 * level6 / data.size());
    }

    // Test level 9
    {
        auto result = zlib.compress(data, 9);
        ASSERT_TRUE(result.has_value());
        bestCompression = result.value().size();
        test::verbosePrint("Level 9 (best): {} bytes (ratio: {:.2f}%)\n", bestCompression, 100.0 * bestCompression / data.size());
    }

    EXPECT_LE(fastestCompression, data.size()) << "Compression level 1 should not expand data (for compressible text)";
    EXPECT_LE(bestCompression, fastestCompression) << "Level 9 should compress at least as well as level 1";
    EXPECT_GE(noCompression, data.size()) << "Level 0 may produce slightly larger output";

    auto invalidResult = zlib.compress(data, 10);
    EXPECT_FALSE(invalidResult.has_value()) << "Should reject invalid compression level 10";

    invalidResult = zlib.compress(data, -2);
    EXPECT_FALSE(invalidResult.has_value()) << "Should reject invalid compression level -2";
}
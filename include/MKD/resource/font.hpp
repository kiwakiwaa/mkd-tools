//
// kiwakiwaaにより 2026/02/01 に作成されました。
//

#pragma once

#include "MKD/result.hpp"

#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <span>
#include <vector>

namespace fs = std::filesystem;

namespace MKD
{
    /*
    * The font data is reconstructed by concatenating all RSC entries in order
    */
    class Font
    {
    public:
        /**
         * Loads a font from a directory containing RSC fragments
         * @param directoryPath Path to directory
         * @return Font instance with complete font data or error string
         */
        static Result<Font> load(const fs::path& directoryPath);

        /**
         * Returns the font name (derived from the source directory)
         * @return Reference to the font name
         */
        [[nodiscard]] const std::string& name() const noexcept;

        /**
         * Provides read-only access to the complete font data
         * @return Span containing the complete font file data
         */
        [[nodiscard]] Result<std::span<const uint8_t>> getData() const;

        /**
         * Attempts to detect the font file type from its magic bytes
         * @return String containing "ttf" or "otf" if detectable
         */
        [[nodiscard]] std::optional<std::string> detectType() const;

        /**
         * Checks if the font contains any data
         * @return true if no data was loaded
         */
        [[nodiscard]] bool isEmpty() const noexcept;

    private:
        /*
         * Private constructor - use load()
         */
        explicit Font(std::string name, std::vector<uint8_t> data);

        std::string name_;
        std::vector<uint8_t> data_;
    };
}

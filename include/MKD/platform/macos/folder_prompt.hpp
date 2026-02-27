//
// kiwakiwaaにより 2026/02/27 に作成されました。
//

#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace MKD::macOS
{
    struct FolderPromptOptions
    {
        std::string message = "Select a folder";
        std::string confirmButton = "Select";
        std::optional<std::filesystem::path> initialDirectory;
    };

    /*
     * Activate this process so the panel appears in front
     * needed for CLI
     */
    void activateAsAccessory();

    /*
     * Show an NSOpenPanel for directory selection
     */
    std::optional<std::filesystem::path> promptForFolder(const FolderPromptOptions& options);
}

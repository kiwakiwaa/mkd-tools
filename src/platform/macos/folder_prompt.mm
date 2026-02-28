#include "MKD/platform/macos/folder_prompt.hpp"

#import <Cocoa/Cocoa.h>

namespace MKD::macOS
{
    void activateAsAccessory()
    {
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];
        [NSApp activateIgnoringOtherApps:YES];
    }

    std::optional<std::filesystem::path> promptForFolder(const FolderPromptOptions& options)
    {
        @autoreleasepool
        {
            NSOpenPanel* panel = [NSOpenPanel openPanel];
            panel.message = [NSString stringWithUTF8String:options.message.c_str()];
            panel.prompt  = [NSString stringWithUTF8String:options.confirmButton.c_str()];
            panel.canChooseFiles = NO;
            panel.canChooseDirectories = YES;
            panel.allowsMultipleSelection = NO;

            if (options.initialDirectory)
            {
                NSString* path = [NSString stringWithUTF8String:options.initialDirectory->c_str()];
                panel.directoryURL = [NSURL fileURLWithPath:path];
            }

            if ([panel runModal] != NSModalResponseOK)
                return std::nullopt;

            NSURL* selected = panel.URLs.firstObject;
            if (!selected) return std::nullopt;

            return std::filesystem::path{selected.path.UTF8String};
        }
    }
}
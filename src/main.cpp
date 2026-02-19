#include <iostream>
#include <indicators.h>

#include "monokakido/dictionary/dictionary.hpp"
#include "monokakido/platform/directory_dictionary_source.hpp"

#ifdef __APPLE__
#include "monokakido/platform/macos/macos_dictionary_source.hpp"
#endif


static void printUsage(const char* program, const monokakido::DictionarySource& source)
{
    std::cerr << "Available dictionaries:\n";
    if (auto available = source.findAllAvailable())
    {
        std::ranges::sort(*available, [](const auto& a, const auto& b) { return a.id < b.id; });
        for (const auto& [id, path] : *available)
            std::cout << "  " << id << '\n';
    }
    else
    {
        std::cerr << "  (failed to list: " << available.error() << ")\n";
    }

#ifdef __APPLE__
    std::cerr << "\nUsage:\n"
              << "  " << program << " <dictionary_name>\n"
              << "  " << program << " --dir <dictionaries_path> <dictionary_name>\n";
#else
    std::cerr << "\nUsage: " << program << " <dictionaries_path> <dictionary_name>\n";
#endif
}


int main(int argc, char* argv[])
{
    std::unique_ptr<monokakido::DictionarySource> source;
    const char* dictName = nullptr;

#ifdef __APPLE__
    if (argc >= 3 && std::string(argv[1]) == "--dir")
    {
        source = std::make_unique<monokakido::DirectoryDictionarySource>(argv[2]);
        if (argc >= 4)
            dictName = argv[3];
    }
    else
    {
        auto macSource = std::make_unique<monokakido::MacOSDictionarySource>();
        if (macSource->checkAccess() == monokakido::MacOSDictionarySource::AccessStatus::NeedsPermission)
        {
            std::cout << "Need permission to access dictionaries folder.\n";
            if (!macSource->requestAccess())
            {
                std::cerr << "Permission denied by user.\n";
                return 1;
            }
            std::cout << "Access granted!\n";
        }
        source = std::move(macSource);
        if (argc >= 2)
            dictName = argv[1];
    }
#else
    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0] << " <dictionaries_path> [dictionary_name]\n";
        return 1;
    }
    source = std::make_unique<monokakido::DirectoryDictionarySource>(argv[1]);
    if (argc >= 3)
        dictName = argv[2];
#endif

    if (!dictName)
    {
        printUsage(argv[0], *source);
        return 0;
    }

    const auto dict = monokakido::Dictionary::open(dictName, *source);
    if (!dict)
    {
        std::cerr << "Failed to open dictionary: " << dict.error() << std::endl;
        return 1;
    }

    const auto result = dict->exportAll({
        .outputDirectory = std::filesystem::path(std::getenv("HOME")) / "Documents" / "test"
    });

    for (const auto& error : result.errors)
        std::cerr << "Error: " << error << std::endl;

    return result.errors.empty() ? 0 : 1;
}
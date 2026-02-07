#include <iostream>
#include <indicators.h>

#include "monokakido/core/platform/fs.hpp"
#include "monokakido/dictionary/catalog.hpp"
#include "monokakido/dictionary/dictionary.hpp"

int main(int argc, char* argv[])
{
    const auto& catalog = monokakido::DictionaryCatalog::instance();

    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0] << " <dictionary_name>\n";
        std::cerr << "Available dictionaries: \n";
        if (auto available = catalog.findAllAvailable())
        {
            std::ranges::sort(*available, [](const auto& dict1, const auto& dict2) { return dict1.id < dict2.id; });
            for (auto it = available->begin(); it != available->end(); ++it)
                std::cout << it->id << '\n';

            std::cout << std::endl;
        }
        return 0;
    }

    if (const auto status = catalog.checkAccess(); status == monokakido::DictionaryCatalog::AccessStatus::NeedsPermission) {
        std::cout << "Need permission to access dictionaries folder.\n";

        if (!catalog.requestAccess()) {
            std::cerr << "Permission denied by user\n";
            return 1;
        }

        std::cout << "Access granted!\n";
    }

    const auto dict = monokakido::Dictionary::open(argv[1]);
    if (!dict)
    {
        std::cerr << "Failed to open dictionary: " << dict.error() << std::endl;
        return 0;
    }

    auto result = dict->exportAll({.outputDirectory = std::filesystem::path(std::getenv("HOME")) / "Documents" / "test"});
    for (auto& error : result.errors)
    {
        std::cerr << "Error: " << error << std::endl;
    }

    return 0;
}
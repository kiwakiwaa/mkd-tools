#include <iostream>

#include "monokakido/core/platform/fs.hpp"
#include "monokakido/dictionary/catalog.hpp"
#include "monokakido/dictionary/dictionary.hpp"

using namespace monokakido;

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0] << " <dictionary_name>\n";
        std::cerr << "Available dictionaries: \n";

        const auto& catalog = dictionary::DictionaryCatalog::instance();
        if (const auto status = catalog.checkAccess(); status == dictionary::DictionaryCatalog::AccessStatus::NeedsPermission) {
            std::cout << "Need permission to access dictionaries folder.\n";

            if (!catalog.requestAccess()) {
                std::cerr << "Permission denied by user\n";
                return 1;
            }

            std::cout << "Access granted!\n";
        }

        const auto dict = dictionary::Dictionary::open("KANKENKJ2");
        if (!dict)
        {
            std::cerr << "Failed to open dictionary: " << dict.error() << std::endl;
            return 0;
        }

        dict->print();
        if (auto result = dict->exportAllResources(); !result)
        {
            std::cerr << "Failed to export resources: " << result.error() << std::endl;
        }
    }

    return 0;
}
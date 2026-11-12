//
// kiwakiwaaにより 2026/02/20 に作成されました。
//

#include "MKDCLI/cli.hpp"
#include "MKDCLI/commands.hpp"
#include "MKDCLI/source.hpp"

#include <clocale>
#include <exception>
#include <iostream>
#include <type_traits>

int main(int argc, char* argv[])
{
    // Indicators uses C char locale for its glyphs
    std::setlocale(LC_CTYPE, "");

    try
    {
        MKDCLI::CLIApp app;
        auto invocation = app.parse(argc, argv, std::cout, std::cerr);
        if (!invocation)
            return invocation.error();

#ifdef __APPLE__
        if (const auto* command = std::get_if<MKDCLI::InstallCommand>(&invocation->command))
            return MKDCLI::runCommand(*command);
#endif

        if (const auto* command = std::get_if<MKDCLI::ValidateCommand>(&invocation->command))
            return MKDCLI::runCommand(*command);

        auto source = MKDCLI::createSource(invocation->global);
        if (!source)
        {
            std::cerr << "Error: " << source.error() << "\n";
            return 1;
        }

        return std::visit([&]<typename Command>(const Command& command)
        {
            if constexpr (std::is_same_v<Command, MKDCLI::ListCommand>)
                return MKDCLI::runCommand(**source, command);
            else if constexpr (std::is_same_v<Command, MKDCLI::InstallCommand> ||
                               std::is_same_v<Command, MKDCLI::ValidateCommand>)
                return 1;
            else
            {
                auto info = (*source)->findById(command.productId);
                if (!info)
                {
                    std::cerr << "Error: " << info.error() << "\n";
                    return 1;
                }
                auto product = MKD::DictionaryProduct::openAtPath(info->path);
                if (!product)
                {
                    std::cerr << "Error: " << product.error() << "\n";
                    return 1;
                }
                return MKDCLI::runCommand(*product, command);
            }
        }, invocation->command);
    }
    catch (const std::exception& error)
    {
        std::cerr << "Error: " << error.what() << "\n";
        return 1;
    }
}

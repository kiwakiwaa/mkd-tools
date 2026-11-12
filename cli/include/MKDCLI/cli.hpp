//
// kiwakiwaaにより 2026/02/20 に作成されました。
//

#pragma once

#include "MKD/output/export_options.hpp"
#include "MKD/dictionary/authoring_diagnostics.hpp"

#include <CLI/CLI.hpp>

#include <expected>
#include <filesystem>
#include <iosfwd>
#include <string>
#include <variant>

namespace MKDCLI
{
    struct GlobalOptions
    {
        std::filesystem::path directory;
    };

    struct ListCommand
    {
    };

    struct InfoCommand
    {
        std::string productId;
    };

    enum class ExportFormat
    {
        Directory,
        Tar
    };

    struct ExportCommand
    {
        std::string productId;
        MKD::ExportOptions options;
        ExportFormat format = ExportFormat::Directory;
        std::filesystem::path outputDirectory;
    };

    struct InstallCommand
    {
        std::filesystem::path package;
        bool replace = false;
    };

    struct ValidateCommand
    {
        std::filesystem::path package;
        MKD::ValidationOptions options;
    };

    struct Invocation
    {
        GlobalOptions global;
        std::variant<ListCommand, InfoCommand, ExportCommand, InstallCommand, ValidateCommand> command;
    };

    class CLIApp
    {
    public:
        CLIApp();

        /**
         * Parse without opening products or requesting access. Help, version and
         * invalid arguments return an exit status after writing to the supplied streams.
         */
        std::expected<Invocation, int> parse(int argc, char* argv[],
                                             std::ostream& out, std::ostream& err);

    private:
        void addInfoCommand();
        void addValidateCommand();
        void addInstallCommand();
        void addExportCommand();
        void addResourceOptions(CLI::App& command);

        CLI::App app_{"mkd-tools"};
        Invocation invocation_;
        InfoCommand info_;
        ValidateCommand validate_;
        InstallCommand install_;
        ExportCommand export_;
    };
}

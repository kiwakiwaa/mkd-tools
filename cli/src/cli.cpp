//
// kiwakiwaaにより 2026/02/20 に作成されました。
//

#include "MKDCLI/cli.hpp"

#include <map>
#include <ostream>

namespace MKDCLI
{
    CLIApp::CLIApp()
    {
        app_.require_subcommand(0, 1);
        app_.set_version_flag("--version", "mkd-tools " MKD_CLI_VERSION);
        app_.add_option("-d,--dir", invocation_.global.directory,
                        "Use a directory as the dictionary source")->check(CLI::ExistingDirectory);
        app_.add_subcommand("list", "List available dictionary products")->fallthrough();
        addInfoCommand();
        addValidateCommand();
        addExportCommand();
#ifdef __APPLE__
        addInstallCommand();
#endif
    }

    void CLIApp::addValidateCommand()
    {
        auto* command = app_.add_subcommand("validate", "Check a finished package without modifying it");
        command->add_option("package", validate_.package, "Path to a finished package")->required();
        command->add_option("--max-issues", validate_.options.maxIssues, "Maximum reported issues (counts remain complete)");
        command->add_flag("--warnings-as-errors", validate_.options.warningsAsErrors, "Fail validation on warnings");
        command->add_flag("--scan-all-documents", validate_.options.scanAllDocuments, "Also check unregistered loose HTML documents");
        command->callback([this] { invocation_.command = std::move(validate_); });
    }

    void CLIApp::addInstallCommand()
    {
        auto* command = app_.add_subcommand("install", "Install an mkd-created product in Monokakido Dictionaries app");
        command->add_option("package", install_.package, "Path to a finished mkd package")->required()->check(CLI::ExistingDirectory);
        command->add_flag("--replace", install_.replace, "Replace an installed mkd package");
        command->callback([this] { invocation_.command = std::move(install_); });
    }

    void CLIApp::addInfoCommand()
    {
        auto* command = app_.add_subcommand("info", "Show a product and its dictionary contents");
        command->fallthrough();
        command->add_option("product-id", info_.productId, "Product identifier (see 'list')")->required();
        command->callback([this] { invocation_.command = std::move(info_); });
    }

    void CLIApp::addExportCommand()
    {
        auto* command = app_.add_subcommand("export", "Export a product's dictionary resources");
        command->fallthrough();
        command->add_option("product-id", export_.productId, "Product identifier (see 'list')")->required();
        command->callback([this] { invocation_.command = std::move(export_); });

        const std::map<std::string, ExportFormat> formats{
            {"directory", ExportFormat::Directory}, {"tar", ExportFormat::Tar}
        };
        auto formatNames = CLI::IsMember(formats);
        command->add_option("--format", export_.format, "Output format (default: directory)")
               ->transform((formatNames & CLI::Transformer(formats)).description(formatNames.get_description()))
               ->type_name("FORMAT");
        auto& options = export_.options;
        command->add_option("-o,--output", export_.outputDirectory,
                            "Output directory (default: ./mkd-export/<product-id>)");
        command->add_flag("--overwrite", options.overwriteExisting, "Overwrite existing files");
        command->add_flag("--pretty", options.prettyPrintXml, "Pretty-print XML entry content");

        const std::map<std::string, MKD::KeystoreExportMode> modes{
            {"forward", MKD::KeystoreExportMode::Forward},
            {"inverse", MKD::KeystoreExportMode::Inverse},
            {"both", MKD::KeystoreExportMode::Both},
        };
        auto names = CLI::IsMember(modes);
        command->add_option("--keystore-mode", options.keystoreExportMode,
                            "Keystore export mode (default: inverse)")
               ->transform((names & CLI::Transformer(modes)).description(names.get_description()))
               ->type_name("MODE");
        addResourceOptions(*command);
    }

    void CLIApp::addResourceOptions(CLI::App& command)
    {
        const std::map<std::string, MKD::ResourceType> resources{
            {"audio", MKD::ResourceType::Audio},
            {"entries", MKD::ResourceType::Contents},
            {"appendix-entries", MKD::ResourceType::AppendixEntries},
            {"graphics", MKD::ResourceType::Graphics},
            {"fonts", MKD::ResourceType::Fonts},
            {"keystores", MKD::ResourceType::Keystores},
            {"headlines", MKD::ResourceType::Headlines},
        };
        auto names = CLI::IsMember(resources);
        command.add_option("--only", export_.options.resources, "Export only this resource type (repeatable)")
               ->transform((names & CLI::Transformer(resources)).description(names.get_description()))
               ->type_name("RESOURCE")->type_size(1)->expected(1)->allow_extra_args(false)
               ->multi_option_policy(CLI::MultiOptionPolicy::TakeAll);
    }

    std::expected<Invocation, int> CLIApp::parse(int argc, char* argv[],
                                                 std::ostream& out, std::ostream& err)
    {
        try
        {
            app_.parse(argc, argv);
            if (std::holds_alternative<ValidateCommand>(invocation_.command) && !invocation_.global.directory.empty())
                throw CLI::ValidationError("--dir", "does not apply to package validation");
            if (std::holds_alternative<InstallCommand>(invocation_.command) && !invocation_.global.directory.empty())
                throw CLI::ValidationError("--dir", "does not apply to installation");
        }
        catch (const CLI::ParseError& error)
        {
            return std::unexpected(app_.exit(error, out, err));
        }

        if (app_.get_subcommands().empty())
        {
            out << app_.help();
            return std::unexpected(0);
        }

        if (auto* command = std::get_if<ExportCommand>(&invocation_.command);
            command && command->outputDirectory.empty())
        {
            command->outputDirectory = std::filesystem::path("mkd-export") / command->productId;
        }
        return std::move(invocation_);
    }
}

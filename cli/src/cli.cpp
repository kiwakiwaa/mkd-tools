//
// kiwakiwaaにより 2026/02/20 に作成されました。
//

#include "MKDCLI/cli.hpp"
#include "MKDCLI/format.hpp"
#include "MKD/platform/directory_dictionary_source.hpp"

#ifdef __APPLE__
#include "MKD/platform/macos/macos_dictionary_source.hpp"
#endif

#include <algorithm>
#include <cstdlib>
#include <iostream>

namespace MKDCLI
{
    std::optional<MKD::ResourceType> parseResourceType(std::string_view str) noexcept
    {
        if (str == "audio") return MKD::ResourceType::Audio;
        if (str == "entries") return MKD::ResourceType::Entries;
        if (str == "graphics") return MKD::ResourceType::Graphics;
        if (str == "fonts") return MKD::ResourceType::Fonts;
        if (str == "keystores") return MKD::ResourceType::Keystores;
        if (str == "headlines") return MKD::ResourceType::Headlines;
        return std::nullopt;
    }


    namespace
    {
        bool isFlag(std::string_view arg, std::string_view shortForm, std::string_view longForm)
        {
            return arg == shortForm || arg == longForm;
        }

        // Try to consume a flag's value from argv
        std::expected<std::optional<std::string_view>, std::string> consumeValue(
            std::span<char*> args, size_t& i,
            std::string_view shortForm,
            std::string_view longForm)
        {
            std::string_view arg = args[i];
            if (arg != shortForm && arg != longForm)
                return std::optional<std::string_view>{std::nullopt};

            if (i + 1 >= args.size())
                return std::unexpected(std::string("Expected value after ") + std::string(arg));

            ++i;
            return std::optional<std::string_view>{args[i]};
        }
    }

    std::expected<CLIApp, std::string> CLIApp::parse(int argc, char* argv[])
    {
        CLIOptions opts;
        std::string program = argc > 0 ? fs::path(argv[0]).filename().string() : "mkd";

        auto args = std::span(argv, static_cast<size_t>(argc));
        bool commandParsed = false;

        for (size_t i = 1; i < args.size(); ++i)
        {
            std::string_view arg = args[i];

            if (isFlag(arg, "-h", "--help"))
            {
                opts.command = Command::Help;
                return CLIApp(std::move(opts), std::move(program));
            }

            if (arg == "--version")
            {
                opts.command = Command::Version;
                return CLIApp(std::move(opts), std::move(program));
            }

            if (arg == "--no-colour")
            {
                opts.noColour = true;
                continue;
            }

            if (auto val = consumeValue(args, i, "-d", "--dir"); !val)
                return std::unexpected(val.error());
            else if (val->has_value())
            {
                opts.dirOverride = fs::path(std::string(val->value()));
                continue;
            }

            if (!commandParsed)
            {
                if (isFlag(arg, "-l", "--list") || arg == "list")
                {
                    opts.command = Command::List;
                    commandParsed = true;
                    continue;
                }

                if (arg == "info")
                {
                    opts.command = Command::Info;
                    commandParsed = true;
                    continue;
                }

                if (isFlag(arg, "-e", "--export") || arg == "export")
                {
                    opts.command = Command::Export;
                    commandParsed = true;
                    continue;
                }

                // bare dictionary ID implies export
                if (!arg.starts_with('-'))
                {
                    opts.command = Command::Export;
                    opts.dictId = std::string(arg);
                    commandParsed = true;
                    continue;
                }

                return std::unexpected("Unknown option: " + std::string(arg));
            }

            if (opts.command == Command::Export)
            {
                if (auto val = consumeValue(args, i, "-o", "--output"); !val)
                    return std::unexpected(val.error());
                else if (val->has_value())
                {
                    opts.outputDir = fs::path(std::string(val->value()));
                    continue;
                }

                if (arg == "--overwrite")
                {
                    opts.overwrite = true;
                    continue;
                }

                if (arg == "--pretty")
                {
                    opts.prettyPrintXml = true;
                    continue;
                }

                if (auto val = consumeValue(args, i, "", "--keystore-mode"); !val)
                    return std::unexpected(val.error());
                else if (val->has_value())
                {
                    if (auto mode = val->value(); mode == "forward") opts.keystoreMode = MKD::KeystoreExportMode::Forward;
                    else if (mode == "inverse") opts.keystoreMode = MKD::KeystoreExportMode::Inverse;
                    else if (mode == "both") opts.keystoreMode = MKD::KeystoreExportMode::Both;
                    else return std::unexpected("Unknown keystore mode: " + std::string(mode));
                    continue;
                }

                // --only <type>  (can be repeated)
                if (auto val = consumeValue(args, i, "", "--only"); !val)
                    return std::unexpected(val.error());
                else if (val->has_value())
                {
                    if (auto rt = parseResourceType(val->value()))
                        opts.onlyResources.push_back(*rt);
                    else
                        return std::unexpected("Unknown resource type: " + std::string(val->value())
                                               + " (expected: audio, entries, graphics, fonts, keystores, headlines)");
                    continue;
                }
            }

            if (!arg.starts_with('-') && opts.dictId.empty())
            {
                opts.dictId = std::string(arg);
                continue;
            }

            return std::unexpected("Unexpected argument: " + std::string(arg));
        }

        if (!commandParsed)
        {
            opts.command = Command::Help;
            return CLIApp(std::move(opts), std::move(program));
        }

        if ((opts.command == Command::Info || opts.command == Command::Export)
            && opts.dictId.empty())
        {
            return std::unexpected("Missing required dictionary ID.\n"
                                   "Run '" + program + " list' to see available dictionaries.");
        }

        if (opts.command == Command::Export && opts.outputDir.empty())
        {
            // default output ~/Documents/mkd-export/<dictId>
            if (const char* home = std::getenv("HOME"))
                opts.outputDir = fs::path(home) / "Documents" / "mkd-export";
            else
                opts.outputDir = fs::current_path() / "mkd-export";
        }

        return CLIApp(std::move(opts), std::move(program));
    }


    std::expected<std::unique_ptr<MKD::DictionarySource>, std::string> CLIApp::createSource() const
    {
        if (options_.dirOverride)
            return std::make_unique<MKD::DirectoryDictionarySource>(*options_.dirOverride);

#ifdef __APPLE__
        auto source = std::make_unique<MKD::MacOSDictionarySource>();
        if (!source->checkAccess())
        {
            std::cerr << Colour::yellow("Requesting access to macOS dictionaries folder...\n");
            if (!source->requestAccess(true))
                return std::unexpected("Permission denied. Use --dir <path> to specify a directory manually.");

            std::cerr << Colour::green("Access granted.\n");
        }

        return source;
#else
        return std::unexpected(
            "No dictionary source specified.\n"
            "Use --dir <path> to point to a dictionaries directory.");
#endif
    }


    MKD::ExportOptions CLIApp::buildExportOptions() const
    {
        return {
            .outputDirectory     = options_.outputDir,
            .overwriteExisting   = options_.overwrite,
            .createSubdirectories = true,
            .prettyPrintXml      = options_.prettyPrintXml,
            .keystoreExportMode  = options_.keystoreMode,
            .resources           = options_.onlyResources,
            .progressCallback    = nullptr,
        };
    }


    const CLIOptions& CLIApp::options() const noexcept
    {
        return options_;
    }


    void CLIApp::printHelp(std::string_view program)
    {
        std::cerr
                << Colour::bold("mkd-tools") << " — Monokakido dictionary toolkit\n\n"

                << Colour::bold("USAGE:\n")
                << "  " << program << " <command> [options]\n"
                << "  " << program << " <dict_id>                "
                << Colour::dim("(shorthand for 'export <dict_id>')") << "\n\n"

                << Colour::bold("COMMANDS:\n")
                << "  list,   -l, --list              List available dictionaries\n"
                << "  info    <dict_id>               Show detailed dictionary info\n"
                << "  export, -e, --export <dict_id>  Export dictionary resources\n"
                << "  help,   -h, --help              Show this help\n"
                << "  --version                       Show version\n\n"

                << Colour::bold("GLOBAL OPTIONS:\n")
                << "  -d, --dir <path>                Use a directory as dictionary source\n"
                << "  --no-u                          Disable coloured output\n\n"

                << Colour::bold("EXPORT OPTIONS:\n")
                << "  -o, --output <path>             Output directory "
                << Colour::dim("(default: ~/Documents/mkd-export)") << "\n"
                << "  --only <type>                   Export only this resource type "
                << Colour::dim("(repeatable)") << "\n"
                << "                                  "
                << Colour::dim("types: audio, entries, graphics, fonts, keystores, headlines") << "\n"
                << "  --overwrite                     Overwrite existing files\n"
                << "  --pretty                        Pretty-print XML entry content\n"
                << "  --keystore-mode <mode>          "
                << Colour::dim("inverse | forward | both") << " (default: inverse)\n\n"

                << Colour::bold("EXAMPLES:\n")
#ifdef __APPLE__
                << "  " << program << " export DAIJISEN2\n"
#else
            << "  " << program << " --dir /path/to/dicts export DAIJISEN2\n"
#endif
                << "\n";
    }

    void CLIApp::printVersion()
    {
        std::cout << "mkd-tools 0.1.0\n";
    }


    CLIApp::CLIApp(CLIOptions options, std::string program)
        : options_(std::move(options)), program_(std::move(program))
    {
    }
}

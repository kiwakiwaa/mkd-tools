//
// kiwakiwaaにより 2026/02/20 に作成されました。
//

#pragma once

#include "MKDCLI/cli.hpp"
#include "MKD/platform/dictionary_source.hpp"
#include "MKD/dictionary/dictionary_product.hpp"

namespace MKDCLI
{
    int runCommand(const ValidateCommand& command);
    int runCommand(const MKD::DictionarySource& source, const ListCommand& command);
#ifdef __APPLE__
    int runCommand(const InstallCommand& command);
#endif
    int runCommand(const MKD::DictionaryProduct& product, const InfoCommand& command);
    int runCommand(const MKD::DictionaryProduct& product, const ExportCommand& command);
}

//
// kiwakiwaaにより 2026/01/17 に作成されました。
//

#pragma once

#include <filesystem>

namespace fs = std::filesystem;

namespace monokakido::resource
{

    enum class NamingStrategy
    {
        PreserveId,
        NormaliseUnicode,
        Sequential
    };

    struct ExportOptions
    {
        fs::path outputDirectory;
        NamingStrategy naming = NamingStrategy::PreserveId;
        bool overwriteExisting = false;
        std::function<bool(std::string_view id)> filter;
    };

    namespace filter
    {
        inline auto byExtension(std::string_view ext)
        {
            return [ext](std::string_view id) {
                const auto pos = id.rfind('.');
                return pos != std::string_view::npos ? id.ends_with(ext) : false;
            };
        }

        inline auto contains(std::string_view term)
        {
            return [term](std::string_view id) {
                return id.find(term) != std::string_view::npos;
            };
        }

        auto anyOf(auto... filters)
        {
            return [=](std::string_view id) {
                return (filters.filter(id) || ...);
            };
        }

        auto allOf(auto... filters)
        {
            return [=](std::string_view node) {
                return (filters(node) && ...);
            };
        }

        auto noneOf(auto... filters)
        {
            return [=](std::string_view node) {
                return !(filters(node) || ...);
            };
        }
    }



}
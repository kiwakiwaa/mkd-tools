//
// kiwakiwaaにより 2026/02/07 に作成されました。
//

#pragma once

#include <gtest/gtest.h>
#include <iostream>

namespace monokakido::test
{
    class VerboseTestListener final : public ::testing::EmptyTestEventListener
    {
    public:

        void OnTestStart(const ::testing::TestInfo& test_info) override
        {
            if (verbose_)
            {
                std::cout << "\n▶ Running: " << test_info.test_suite_name() << "." << test_info.name() << "\n";
            }
        }

        void OnTestEnd(const ::testing::TestInfo& test_info) override
        {
            if (verbose_ && test_info.result()->Passed()) {
                std::cout << "✓ Passed\n";
            }
        }

        static void setVerbose(const bool verbose) { verbose_ = verbose; }
        static bool isVerbose() { return verbose_; }

    private:
        static inline bool verbose_ = false;
    };

    inline void printSeparator(const char c = '=', const size_t width = 80)
    {
        if (VerboseTestListener::isVerbose()) {
            std::cout << std::string(width, c) << '\n';
        }
    }

    template<typename... Args>
    void verbosePrint(std::format_string<Args...> fmt, Args&&... args)
    {
        if (VerboseTestListener::isVerbose()) {
            std::cout << std::format(fmt, std::forward<Args>(args)...);
        }
    }
}


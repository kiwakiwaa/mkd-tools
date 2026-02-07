//
// kiwakiwaaにより 2026/02/07 に作成されました。
//

#include <gtest/gtest.h>
#include "test_listener.hpp"


int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);

    for (int i = 1; i < argc; ++i)
    {
        if (std::string(argv[i]) == "--verbose" || std::string(argv[i]) == "-v")
        {
            monokakido::test::VerboseTestListener::setVerbose(true);
        }
    }

    auto& listeners = ::testing::UnitTest::GetInstance()->listeners();
    listeners.Append(new monokakido::test::VerboseTestListener);

    return RUN_ALL_TESTS();
}
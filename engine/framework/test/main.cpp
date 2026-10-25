#include <gtest/gtest.h>

#include "ProductProcessHelper.h"

int main(int argc, char *argv[])
{
    if (const auto result = RunProductProcessHelper(argc, argv); result) {
        return *result;
    }
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

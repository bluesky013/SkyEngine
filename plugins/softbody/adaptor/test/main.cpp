#include <gtest/gtest.h>
void RegisterSoftBodyWorldTestMetadata();
int  main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    RegisterSoftBodyWorldTestMetadata();
    return RUN_ALL_TESTS();
}

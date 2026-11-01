#include <framework/compression/Compressor.h>
#include <gtest/gtest.h>

int main(int argc, char *argv[])
{
    sky::CompressionManager::Get();
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

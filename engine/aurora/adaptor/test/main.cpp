#include <aurora/adaptor/AuroraReflection.h>
#include <aurora/adaptor/assets/AuroraAssetReflection.h>
#include <framework/serialization/SerializationContext.h>
#include <gtest/gtest.h>

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    sky::aurora::RegisterAuroraAssetTypes(sky::SerializationContext::Get());
    sky::aurora::RegisterAuroraRuntimeTypes(sky::SerializationContext::Get());
    return RUN_ALL_TESTS();
}

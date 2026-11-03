#include <animation/assets/GraphAsset.h>
#include <framework/serialization/SerializationContext.h>
#include <gtest/gtest.h>

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    sky::animation::RegisterGraphAssets(sky::SerializationContext::Get());
    return RUN_ALL_TESTS();
}

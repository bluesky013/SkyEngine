#include <curve/assets/CurveAsset.h>
#include <framework/serialization/SerializationContext.h>
#include <gtest/gtest.h>

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    sky::curve::RegisterCurveAssets(sky::SerializationContext::Get());
    return RUN_ALL_TESTS();
}

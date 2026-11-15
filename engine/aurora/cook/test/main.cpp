#include <aurora/adaptor/assets/AuroraAssetReflection.h>
#include <aurora/cook/image/ImageCookSettings.h>
#include <framework/serialization/SerializationContext.h>
#include <gtest/gtest.h>

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    sky::aurora::RegisterAuroraAssetTypes(sky::SerializationContext::Get());
    sky::aurora::cook::RegisterImageCookSettings();
    return RUN_ALL_TESTS();
}

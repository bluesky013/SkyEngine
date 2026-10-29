#include <animation/assets/GraphAsset.h>
#include <animation/world/AnimationComponent.h>
#include <framework/serialization/SerializationContext.h>
#include <gtest/gtest.h>

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    sky::animation::AnimationComponent::Reflect(sky::SerializationContext::Get());
    sky::animation::RegisterGraphAssets(sky::SerializationContext::Get());
    return RUN_ALL_TESTS();
}

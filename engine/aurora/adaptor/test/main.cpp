#include <aurora/adaptor/AuroraReflection.h>
#include <framework/serialization/SerializationContext.h>
#include <framework/world/World.h>
#include <gtest/gtest.h>

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    sky::World::Reflect(sky::SerializationContext::Get());
    sky::aurora::AuroraReflection(sky::SerializationContext::Get());
    return RUN_ALL_TESTS();
}

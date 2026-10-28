#include <framework/world/World.h>
#include <gtest/gtest.h>
#include <optional>


std::optional<int> RunApplicationStartupProcessHelper(int argc, char **argv);

int main(int argc, char *argv[])
{
    if (const auto result = RunApplicationStartupProcessHelper(argc, argv); result) {
        return *result;
    }
    testing::InitGoogleTest(&argc, argv);
    sky::World::Reflect(sky::SerializationContext::Get());
    return RUN_ALL_TESTS();
}

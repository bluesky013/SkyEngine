#include <content/adaptor/ContentReflection.h>
#include <framework/serialization/SerializationContext.h>
#include <gtest/gtest.h>

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    sky::ContentReflection(sky::SerializationContext::Get());
    return RUN_ALL_TESTS();
}

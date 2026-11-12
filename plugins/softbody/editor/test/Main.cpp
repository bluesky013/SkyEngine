#include <gtest/gtest.h>
#include <softbody/assets/SoftBodyRecipe.h>

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    sky::softbody::RegisterSoftBodyRecipeTypes();
    return RUN_ALL_TESTS();
}

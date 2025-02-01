//
// Created by blues on 2024/12/30.
//

#include <gtest/gtest.h>
#include <shader/ShaderVariant.h>

using namespace sky;

TEST(ShaderTest, ShaderVariantKeyTest)
{
    ShaderVariantKey key = {};

    key.SetValue(0, 1, 2);
    key.SetValue(28, 35, 127);

    ASSERT_EQ(key.GetValue(28, 35), 127);
    ASSERT_EQ(key.GetValue(0, 1), 2);
}

TEST(ShaderTest, ShaderVariantListTest1)
{
    ShaderVariantList list;

    list.AddEntry({Name("a"), {0, 0}});
    list.AddEntry({Name("b"), {1, 1}});

    auto permutations = list.GeneratePermutation();
    ASSERT_EQ(permutations.size(), 4);

}

TEST(ShaderTest, ShaderVariantListTest2)
{
    ShaderVariantList list;

    list.AddEntry({Name("a"), {0, 0}});
    list.AddEntry({Name("b"), {1, 2}});
    list.AddEntry({Name("c"), {3, 5}});
    list.AddEntry({Name("e"), {11, 11}});

    list.AddDependency(Name("c"), ShaderOptionDependency{Name("a"), ShaderOptionFunc::EQ, 1});
    list.AddDependency(Name("b"), ShaderOptionDependency{Name("c"), ShaderOptionFunc::EQ, 3});

    auto permutations = list.GeneratePermutation();
    ASSERT_EQ(permutations.size(), 8);
}

int main(int argc, char *argv[])
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
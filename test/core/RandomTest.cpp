//
// Created by Zach Lee on 2021/12/4.
//
#include <gtest/gtest.h>
#include <core/math/Sfmt.h>
#include <string>

using namespace sky;

TEST(MathTest, SfmtTest)
{
    SFMTRandom random(0);
    std::cout << random.GenU32() << std::endl;
}
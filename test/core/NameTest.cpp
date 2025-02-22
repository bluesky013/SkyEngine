//
// Created by blues on 2024/11/23.
//
#include <core/name/Name.h>
#include <gtest/gtest.h>
#include <string>
#include <sstream>

#include <unordered_map>
#include <map>

using namespace sky;

static Name GetA() { return Name("abc"); }
static Name GetB() { return Name("def"); }
static Name GetC() { return Name("ghi"); }

TEST(NameTest, NameBaseTest)
{
    std::unordered_map<Name, uint32_t> NAME_MAP_TEST;
    std::map<Name, uint32_t> MAP_TEST;

    auto a = GetA();
    auto b = GetB();
    auto c = GetC();

    ASSERT_EQ(a, std::string_view("abc"));
    ASSERT_EQ(b, std::string_view("def"));
    ASSERT_EQ(c, std::string_view("ghi"));

    ASSERT_NE(a, std::string_view("aaa"));

    std::stringstream ss;
    ss << a;
    ASSERT_EQ(ss.str(), std::string(a.GetStr().data()));
}
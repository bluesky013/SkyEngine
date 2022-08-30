//
// Created by Zach Lee on 2022/3/13.
//

#include <framework/application/SettingRegistry.h>
#include <gtest/gtest.h>

using namespace sky;

TEST(SettingRegistryTest, BasicTest)
{
    SettingRegistry registry;
    registry.SetValue("/test1", 1);
    registry.SetValue("/test2", 2u);
    registry.SetValue("/test3", uint8_t(3));
    registry.SetValue("/test4", int8_t(4));
    registry.SetValue("/test5", uint16_t(5));
    registry.SetValue("/test6", int16_t(6));
    registry.SetValue("/test7", uint64_t(7));
    registry.SetValue("/test8", int64_t(8));
    registry.SetValue("/test9", 9.0);
    registry.SetValue("/test10", 10.f);
    registry.SetValue("/test11", true);
    registry.SetValue("/test12", std::string_view("v1"));

    registry.SetValue("/test1/v1", 3);

    std::string out;
    registry.Save(out);

    std::cout << out << std::endl;
}
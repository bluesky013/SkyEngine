//
// Created by Zach Lee on 2021/11/7.
//
#include "vulkan/Driver.h"

int main()
{
    using namespace sky::drv;

    auto driver = Driver::Create({"", "", true});

    driver->CreateDevice({});

    Driver::Destroy(driver);
    return 0;
}
//
// Created by Zach Lee on 2021/11/11.
//

#pragma once

#ifdef _DEBUG
#include <cassert>
#define SKY_ASSERT(val) assert(val);
#else
#define SKY_ASSERT(val)
#endif

#ifdef _MSC_VER
    #define SKY_EXPORT __declspec(dllexport)
    #define SKY_IMPORT __declspec(dllimport)
#else
    #define SKY_EXPORT __attribute__((visibility("default")))
    #define SKY_IMPORT __attribute__((visibility("default")))
#endif

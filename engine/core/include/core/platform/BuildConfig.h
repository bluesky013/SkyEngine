//
// Created by Zach Lee on 2026/1/3.
//

#pragma once

// Developer facilities are independent of compiler configuration and CRT selection.
#ifndef SKY_DEVELOP
    #define SKY_DEVELOP 0
#endif

#ifndef SKY_BUILD_DEVELOP
    #define SKY_BUILD_DEVELOP 0
#endif

#ifndef SKY_ENABLE_ASSERT
    #if defined(_DEBUG) || SKY_BUILD_DEVELOP
        #define SKY_ENABLE_ASSERT 1
    #else
        #define SKY_ENABLE_ASSERT 0
    #endif
#endif

#ifndef SKY_ENABLE_GPU_VALIDATION
    #if defined(_DEBUG)
        #define SKY_ENABLE_GPU_VALIDATION 1
    #else
        #define SKY_ENABLE_GPU_VALIDATION 0
    #endif
#endif

// Resource names follow the facilities intent unless explicitly overridden.
#ifndef SKY_ENABLE_RESOURCE_NAME
    #if SKY_DEVELOP
        #define SKY_ENABLE_RESOURCE_NAME 1
    #else
        #define SKY_ENABLE_RESOURCE_NAME 0
    #endif
#endif

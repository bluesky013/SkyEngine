//
// Created by Zach Lee on 2026/1/3.
//

#pragma once

// SKY_DEVELOP marks a "develop" build intent (editor/tooling), orthogonal to
// _DEBUG which only describes the compiler configuration. Developer-facing
// facilities (resource names, extra validation, debug UI) gate on SKY_DEVELOP.
#ifndef SKY_DEVELOP
#define SKY_DEVELOP 0
#endif

// Set debug names on backend GPU resources (VkBuffer/VkImage, etc.) so
// debuggers (RenderDoc/NSight/Xcode) can show readable names. Gated on
// SKY_DEVELOP to keep release builds free of this cost.
#ifndef SKY_ENABLE_RESOURCE_NAME
#if SKY_DEVELOP
#define SKY_ENABLE_RESOURCE_NAME 1
#else
#define SKY_ENABLE_RESOURCE_NAME 0
#endif
#endif

#if SKY_EDITOR
    #define EDITABLE(stat) (stat)
#else
    #define EDITABLE(stat)
#endif
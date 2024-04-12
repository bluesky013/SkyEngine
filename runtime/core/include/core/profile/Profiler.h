//
// Created by blues on 2024/3/1.
//

#pragma once

#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>
#define SKY_PROFILE_FRAME FrameMark
#define SKY_PROFILE_SCOPE ZoneScoped
#define SKY_PROFILE_NAME(name) ZoneScopedN(name)
#else
#define SKY_PROFILE_FRAME
#define SKY_PROFILE_SCOPE
#define SKY_PROFILE_NAME(name)
#endif
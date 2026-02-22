//
// Created by blues on 2024/3/1.
//

#pragma once

#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>
#define SKY_FRAME_MARK(name)          FrameMarkNamed(name);
#define SKY_PROFILE_SCOPE             ZoneScoped;
#define SKY_PROFILE_NAME(name)        ZoneScopedN(name); // NOLINT
#else
#define SKY_FRAME_MARK(name)
#define SKY_PROFILE_SCOPE
#define SKY_PROFILE_NAME(name)
#endif
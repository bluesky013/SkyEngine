//
// Created on 2026/09/21.
//

#pragma once

#include <core/math/Vector3.h>

namespace sky {

    class AudioListener {
    public:
        AudioListener()          = default;
        virtual ~AudioListener() = default;

        virtual void SetPosition(const Vector3 &position) = 0;
        virtual void SetOrientation(const Vector3 &forward, const Vector3 &up) = 0;
    };

} // namespace sky

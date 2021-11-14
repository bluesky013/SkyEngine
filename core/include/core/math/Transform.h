//
// Created by Zach Lee on 2021/11/13.
//


#pragma once

#include <core/math/Math.h>
#include <core/math/Vector.h>
#include <core/math/Quaternion.h>

namespace sky {

    struct Transform {
        Vector3 pos;
        Vector3 scale;
        Quaternion rotation;
    };

}
//
// Created by Zach Lee on 2021/11/13.
//


#pragma once

#include <core/math/Math.h>
#include <core/math/Vector.h>
#include <core/math/Quaternion.h>

namespace sky {

    struct Transform {
        Vector3 pos = {0, 0, 0};
        Vector3 scale = {1, 1, 1};
        Quaternion rotation = {1, 0, 0, 0};
    };

}
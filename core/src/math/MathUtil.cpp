//
// Created by Zach Lee on 2022/7/30.
//

#include <core/math/MathUtil.h>
#include <core/logger/Logger.h>

namespace sky {

    void MathUtil::PrintMatrix(const Matrix4& m)
    {
        LOG_I("MathUtil", "\n[%f, %f, %f, %f]\n[%f, %f, %f, %f]\n[%f, %f, %f, %f]\n[%f, %f, %f, %f]\n",
              m[0][0], m[0][1], m[0][2], m[0][3],
              m[1][0], m[1][1], m[1][2], m[1][3],
              m[2][0], m[2][1], m[2][2], m[2][3],
              m[3][0], m[3][1], m[3][2], m[3][3]);
    }

}
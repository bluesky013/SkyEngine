//
// Created by blues on 2024/9/6.
//

#include <recast/RecastNaviMeshGenerator.h>
#include <recast/RecastConversion.h>
#include <core/math/MathUtil.h>

namespace sky::ai {

    RecastNaviMeshGenerator::RecastNaviMeshGenerator(const CounterPtr<RecastNaviMesh>& mesh)
        : navMesh(mesh)
    {

    }

    bool RecastNaviMeshGenerator::DoWork()
    {
        return true;
    }

    void RecastNaviMeshGenerator::OnComplete(bool result)
    {

    }

} // namespace sky::ai
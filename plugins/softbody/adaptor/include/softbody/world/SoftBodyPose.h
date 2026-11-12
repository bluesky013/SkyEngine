#pragma once

#include <core/math/Matrix4.h>
#include <softbody/assets/SoftBodyAsset.h>

namespace sky::softbody {
    phy::PhysicsStatus BuildSoftBodyPose(const SoftBodyAssetData &asset,
                                         std::span<const Vector3> worldPositions,
                                         const Transform         &root,
                                         std::span<const Matrix4> baseModels,
                                         std::vector<Matrix4>    &models,
                                         std::vector<Matrix4>    &modelNormals);
}

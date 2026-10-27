#pragma once

#include <aurora/adaptor/assets/MeshAsset.h>
#include <core/shapes/TriangleMesh.h>
#include <string>

namespace sky::phy {
    CounterPtr<TriangleMesh> ConvertAuroraCollisionMesh(const Asset<aurora::Mesh> &asset, std::string &error);
}

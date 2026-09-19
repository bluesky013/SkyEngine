//
// Mesh source import via assimp (see MeshProcess.h for the data model).
//

#pragma once

#include <aurora/cook/mesh/MeshProcess.h>

#include <string>
#include <vector>

namespace sky::aurora::cook {

    // Imports a mesh source file (gltf/glb/fbx/obj) from memory. All primitives
    // are extracted with baked node transforms; skinning (bones) is collected
    // into out.skin and per-vertex joints/weights attributes.
    bool LoadMeshSource(const std::vector<uint8_t> &bytes, const std::string &ext, CookMeshSource &out);

} // namespace sky::aurora::cook

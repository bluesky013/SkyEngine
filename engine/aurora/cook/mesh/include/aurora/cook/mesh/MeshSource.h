//
// Mesh source import via assimp (see MeshProcess.h for the data model).
//

#pragma once

#include <aurora/cook/mesh/MeshProcess.h>

#include <string>
#include <vector>

namespace sky::aurora::cook {

    // Import retains one entry per original binding. Static primitives share one entry.
    MeshCookStatus LoadMeshSources(const std::vector<uint8_t> &bytes, const std::string &ext, std::vector<CookMeshSource> &out);
    bool           LoadMeshSource(const std::vector<uint8_t> &bytes, const std::string &ext, CookMeshSource &out);

} // namespace sky::aurora::cook

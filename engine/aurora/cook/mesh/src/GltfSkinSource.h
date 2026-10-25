#pragma once
#include <aurora/cook/mesh/MeshProcess.h>
namespace sky::aurora::cook {
    MeshCookStatus PrepareGltfSkinSource(const std::vector<uint8_t> &bytes, const std::string &extension, std::vector<uint8_t> &out);
}

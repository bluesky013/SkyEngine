#pragma once

#include <aurora/rhi/Core.h>

#include <cstdint>
#include <string>
#include <vector>

namespace sky::aurora {
    // Source records have no result IDs; all other instructions and the module header remain exact.
    // Failure preserves code and nativeEntry.
    bool FinalizeSpirvArtifact(std::vector<uint8_t> &code, ShaderStageFlagBit stage, std::string &nativeEntry, std::string &error);
} // namespace sky::aurora

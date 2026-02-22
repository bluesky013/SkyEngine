//
// Created by blues on 2026/2/19.
//

#include <render/RenderPrimitive.h>

namespace sky {

    bool RenderTechniqueInstance::UpdateProgram(const ShaderVariantKey& pipelineKey)
    {
        if (const ShaderVariantKey final = pipelineKey | vertexKey | batchKey; final != cacheFinalKey) {
            program = technique->RequestProgram(final, false);
            cacheFinalKey = final;
            return !!program;
        }

        return false;
    }

} // namespace sky
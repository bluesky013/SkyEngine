#pragma once

#include <aurora/renderer/RenderResourceTypes.h>
#include <aurora/shader/ShaderFileSystem.h>
#include <aurora/shader/ShaderRef.h>

namespace sky::aurora {
    void                 RegisterSceneShaderSources(ShaderFileSystem &files);
    std::vector<uint8_t> SceneBatchBytes(const Vector4 &tint);
    // Scene techniques use VSMain/FSMain, or <entry>VS/<entry>FS when an entry prefix is supplied.
    std::variant<RendererError, RenderMaterialData> CompileSceneMaterial(ShaderFileSystem &files, API backend, const ShaderRef &shader = {});
} // namespace sky::aurora

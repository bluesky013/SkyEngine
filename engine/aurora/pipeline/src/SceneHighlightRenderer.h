//
// Created on 2026/10/23.
//

#pragma once

#include <aurora/pipeline/SceneHighlight.h>
#include <aurora/renderer/IRenderFeature.h>
#include <core/file/FileSystem.h>

namespace sky::aurora {
    class ShaderFileSystem;
    bool                         IsSceneHighlightInput(const FrameExtensionPayload &input);
    std::optional<RendererError> ValidateSceneHighlightInput(const FrameExtensionPayload &input);
    struct SceneHighlightData {
        SceneHighlightStyle          style;
        std::vector<RenderEntityRef> entities;
    };
    std::variant<FrameError, SceneHighlightData> ReadSceneHighlightInput(const FrameGraphBuildContext &, Name, ViewportId);
    class SceneHighlightRenderer {
    public:
        FrameOperationResult Prepare(FrameFeaturePrepareContext &, ShaderFileSystem &, IFileSystem *);
        FrameOperationResult Render(FrameGraphBuildContext &,
                                    const FrameRenderTarget &,
                                    const Matrix4 &,
                                    GraphImageHandle sceneDepth,
                                    const SceneHighlightStyle &,
                                    std::span<const ResolvedRasterDraw>);
        void                 Reset();

    private:
        ShaderPtr maskShader, outlineShader;
    };
} // namespace sky::aurora

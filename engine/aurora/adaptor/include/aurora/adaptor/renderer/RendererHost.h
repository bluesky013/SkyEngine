//
// Created on 2026/10/17.
//

#pragma once

#include <aurora/renderer/IRenderer.h>
#include <core/file/FileSystem.h>
#include <optional>

namespace sky::aurora {
    class ShaderFileSystem;
    std::optional<RendererError> RegisterScenePass(FramePacket &,
                                                   RenderFeatureId,
                                                   std::shared_ptr<const void>       moduleLease,
                                                   std::shared_ptr<ShaderFileSystem> sources  = {},
                                                   FileSystemPtr                     products = {});
    // One main-thread producer owns the published renderer until shutdown has drained.
    class RendererHost {
    public:
        RendererHost();
        ~RendererHost();
        RendererHost(const RendererHost &)            = delete;
        RendererHost &operator=(const RendererHost &) = delete;

        std::optional<RendererError> Init(IRendererProvider &, const RendererInitInfo &);
        IRenderer                   *Get() const;
        void                         Shutdown();

    private:
        std::unique_ptr<IRenderer> renderer;
    };
} // namespace sky::aurora

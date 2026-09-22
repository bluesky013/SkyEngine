//
// Created on 2026/09/21.
//

#pragma once

#include <aurora/rhi/Core.h>
#include <aurora/rhi/ShaderReflection.h>
#include <core/template/ReferenceObject.h>
#include <ui/IUITextureRegistry.h>
#include <ui/UIDrawData.h>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace sky::aurora {
    class Buffer;
    class CommandBuffer;
    class Device;
    class GraphicsEncoder;
    class GraphicsPipeline;
    class Image;
    class ResourceGroup;
    class Sampler;
    class Shader;
    class ShaderFunction;
} // namespace sky::aurora

namespace sky::ui {

    // UI renderer: owns the GUI pipeline and records draws for `UIDrawData`.
    //
    // The shader is compiled at runtime through `Aurora.Shader` (SPIR-V); the
    // pipeline samples a texture bound via a `ResourceGroup`. This is the engine-
    // level home of the GUI pipeline; hosts (e.g. `SandboxModule`) only call
    // `UpdateDrawData` / `Render`.
    //
    // Conventions (renderer usage; the storage layout of `Matrix4` is row-major
    // in this engine, but the transform convention is fixed HERE, not by the type):
    //   - The renderer treats `Matrix4` with the ROW-VECTOR convention: v' = v * M
    //     (`operator*(Vector4)` == v.x*row0 + v.y*row1 + v.z*row2 + v.w*row3), so
    //     the translation lives in the 4th ROW (m[3].xyz), not the 4th column.
    //   - `UIDrawData` vertices are in pixel space (UI origin top-left, y grows
    //     down). `UpdateDrawData` maps them to clip space with an orthographic
    //     projection: x_clip = 2x/w - 1, and y_clip = +-(2y/h - 1) chosen by the
    //     backend's clip-space Y axis (`DeviceCapability::clipSpaceYDown`: Vulkan
    //     +Y down, D3D12/Metal +Y up), so pixel y=0 always lands at the
    //     framebuffer top.
    //   - `UIVertex::color` is packed ABGR (0xAABBGGRR): the F_RGBA8 vertex
    //     attribute (VK_FORMAT_R8G8B8A8_UNORM) reads the bytes as R,G,B,A, so R
    //     must be in the low byte.
    //   - Textures are referenced by `UITextureId`; the renderer binds the
    //     matching `ResourceGroup` per `UIDrawCmd`. `UI_INVALID_TEXTURE` maps to
    //     the built-in white texture.
    class UIRenderer : public IUITextureRegistry {
    public:
        UIRenderer() = default;
        ~UIRenderer() override;

        UIRenderer(const UIRenderer &) = delete;
        UIRenderer &operator=(const UIRenderer &) = delete;

        bool Init(aurora::Device *device, aurora::PixelFormat colorFormat);
        void Shutdown();

        bool IsValid() const { return pipeline != nullptr; }

        // Update the vertex/index buffers from the given draw data. Vertices are
        // pixel-space (from UIPaintContext); they are converted to NDC on upload.
        void UpdateDrawData(const UIDrawData &drawData, uint32_t surfaceWidth, uint32_t surfaceHeight);

        // Upload built-in / pending textures on first use (records into cmd).
        void EnsureTextureReady(aurora::CommandBuffer *commandBuffer);

        // Record the draws for the given draw data (viewport + scissor + draws).
        void Render(aurora::GraphicsEncoder *encoder, const UIDrawData &drawData, uint32_t surfaceWidth,
                    uint32_t surfaceHeight);

        // IUITextureRegistry: CPU images (icons / atlas pages).
        UITextureId RegisterTexture(const UIImageData &image) override;
        void UpdateTexture(UITextureId id, const UIImageData &image) override;
        void ReleaseTexture(UITextureId id) override;

        // Register an existing GPU image (e.g. a viewport content target). The
        // image must be kept alive by the caller while registered.
        bool RegisterImage(UITextureId id, aurora::Image *image);

    private:
        struct TextureEntry {
            sky::CounterPtr<aurora::Image>         image;
            sky::CounterPtr<aurora::Buffer>        staging;
            sky::CounterPtr<aurora::ResourceGroup> group;
            std::vector<uint8_t>                   pendingPixels; // RGBA8, uploaded on first use
            uint32_t                               width  = 0;
            uint32_t                               height = 0;
            bool                                   uploaded = false;
        };

        bool EnsureBuffers(uint64_t vertexBytes, uint64_t indexBytes);
        aurora::ResourceGroup *CreateTextureGroup(aurora::Image *image);
        aurora::ResourceGroup *FindGroup(UITextureId id) const;
        aurora::GraphicsEncoder *BindPipeline(aurora::GraphicsEncoder *encoder, uint32_t surfaceWidth,
                                              uint32_t surfaceHeight) const;

        aurora::Device                    *device = nullptr;
        aurora::PixelFormat                colorFormat = aurora::PixelFormat::UNDEFINED;
        aurora::ShaderReflection           reflection; // target-specific (backend register/binding)
        sky::CounterPtr<aurora::ShaderFunction>   vs;
        sky::CounterPtr<aurora::ShaderFunction>   ps;
        sky::CounterPtr<aurora::Shader>           shader;
        sky::CounterPtr<aurora::GraphicsPipeline> pipeline;
        sky::CounterPtr<aurora::Buffer>           vertexBuffer;
        sky::CounterPtr<aurora::Buffer>           indexBuffer;
        sky::CounterPtr<aurora::Buffer>           staging;
        sky::CounterPtr<aurora::Sampler>          sampler;
        std::unordered_map<UITextureId, TextureEntry> textures;
        UITextureId nextTextureId = 1;
        uint64_t vertexCapacity = 0;
        uint64_t indexCapacity  = 0;
    };

} // namespace sky::ui

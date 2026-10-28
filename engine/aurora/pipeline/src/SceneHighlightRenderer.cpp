//
// Created on 2026/10/23.
//

#include "SceneHighlightRenderer.h"

#include <aurora/rdg/FrameBindings.h>

#include "SceneBatch.h"
#include "SelectionMaskPass.h"
#include "SelectionOutlinePass.h"
#include <algorithm>
#include <aurora/renderer/PassResourceBindings.h>
#include <aurora/rhi/Device.h>
#include <aurora/shader/ShaderTechnique.h>
#include <cmath>
#include <cstring>
#include <set>
#include <tuple>

namespace sky::aurora {
    namespace {
        constexpr uint32_t MAGIC = 0x53484C31;
        struct Header {
            uint32_t            magic = MAGIC, count = 0;
            SceneHighlightStyle style;
        };
        FrameOperationResult Compile(
            FrameFeaturePrepareContext &context, ShaderFileSystem &sources, IFileSystem *products, API backend, const char *path, ShaderPtr &shader)
        {
            if (shader)
                return FrameOperationComplete{};
            const ShaderTechnique technique{.vertex   = {.relativePath = path, .entry = "VSMain"},
                                            .fragment = {.relativePath = path, .entry = "FSMain"}};
            const auto            compiled = CompileShaderTechnique(sources, backend, technique, products);
            if (const auto *error = std::get_if<RendererError>(&compiled))
                return FrameError{FrameErrorCode::COMPILE_FAILED, error->message};
            const auto created = context.CreateShader(std::get<RenderTechniqueData>(compiled));
            if (const auto *error = std::get_if<FrameError>(&created))
                return *error;
            shader = std::get<ShaderPtr>(created);
            return FrameOperationComplete{};
        }
    } // namespace
    FrameExtensionPayload
    SceneHighlightInput(Name feature, ViewportId output, std::span<const RenderEntityRef> entities, const SceneHighlightStyle &style)
    {
        FrameExtensionPayload result;
        result.feature = feature;
        result.outputs = {output};
        // Keep malformed oversized requests bounded; Validate reports the explicit count error.
        const Header header{MAGIC, static_cast<uint32_t>(std::min<size_t>(entities.size(), 8193)), style};
        result.bytes.resize(sizeof(header) + (header.count <= 8192 ? entities.size_bytes() : 0));
        std::memcpy(result.bytes.data(), &header, sizeof(header));
        if (!entities.empty() && header.count <= 8192)
            std::memcpy(result.bytes.data() + sizeof(header), entities.data(), entities.size_bytes());
        return result;
    }
    bool IsSceneHighlightInput(const FrameExtensionPayload &input)
    {
        uint32_t magic = 0;
        if (input.bytes.size() >= sizeof(magic))
            std::memcpy(&magic, input.bytes.data(), sizeof(magic));
        return magic == MAGIC;
    }
    std::optional<RendererError> ValidateSceneHighlightInput(const FrameExtensionPayload &input)
    {
        const auto invalid = [] { return RendererError{RendererErrorCode::INVALID_ARGUMENT, "Invalid scene highlight input"}; };
        if (input.outputs.size() != 1 || !input.outputs.front().IsValid() || !input.resources.empty() || input.bytes.size() < sizeof(Header))
            return invalid();
        Header header;
        std::memcpy(&header, input.bytes.data(), sizeof(header));
        if (header.magic != MAGIC || header.count > 8192 || input.bytes.size() != sizeof(Header) + header.count * sizeof(RenderEntityRef) ||
            !std::isfinite(header.style.width) || header.style.width < 1 || header.style.width > 4)
            return invalid();
        for (const auto &color : {header.style.visible, header.style.occluded})
            for (float value : color.v)
                if (!std::isfinite(value) || value < 0 || value > 1)
                    return invalid();
        std::set<std::tuple<uint32_t, uint32_t, uint32_t, uint32_t>> identities;
        for (uint32_t i = 0; i < header.count; ++i) {
            RenderEntityRef entity;
            std::memcpy(&entity, input.bytes.data() + sizeof(Header) + i * sizeof(entity), sizeof(entity));
            if (!entity.scene.IsValid() || !entity.entity.IsValid() ||
                !identities.emplace(entity.scene.index, entity.scene.generation, entity.entity.index, entity.entity.generation).second)
                return invalid();
        }
        return {};
    }
    std::variant<FrameError, SceneHighlightData> ReadSceneHighlightInput(const FrameGraphBuildContext &context, Name feature, ViewportId output)
    {
        SceneHighlightData result;
        bool               found = false;
        for (const auto &input : context.GetInputs().prepared->extensions) {
            if (input.feature != feature || !IsSceneHighlightInput(input))
                continue;
            if (const auto error = ValidateSceneHighlightInput(input))
                return FrameError{FrameErrorCode::INVALID_ARGUMENT, error->message};
            if (input.outputs.front() != output)
                continue;
            if (found)
                return FrameError{FrameErrorCode::INVALID_ARGUMENT, "Duplicate scene highlight inputs"};
            found = true;
            Header header;
            std::memcpy(&header, input.bytes.data(), sizeof(header));
            result.style = header.style;
            result.entities.resize(header.count);
            if (header.count)
                std::memcpy(result.entities.data(), input.bytes.data() + sizeof(header), header.count * sizeof(RenderEntityRef));
        }
        return result;
    }
    FrameOperationResult SceneHighlightRenderer::Prepare(FrameFeaturePrepareContext &context, ShaderFileSystem &sources, IFileSystem *products)
    {
        auto mask = maskShader, outline = outlineShader;
        for (const auto &[path, shader] :
             {std::pair{"aurora/pipeline/SelectionMask.slang", &mask}, std::pair{"aurora/pipeline/SelectionOutline.slang", &outline}}) {
            const auto result = Compile(context, sources, products, context.GetRendererInfo().backend, path, *shader);
            if (const auto *error = std::get_if<FrameError>(&result))
                return *error;
        }
        maskShader    = std::move(mask);
        outlineShader = std::move(outline);
        return FrameOperationComplete{};
    }
    FrameOperationResult SceneHighlightRenderer::Render(FrameGraphBuildContext             &context,
                                                        const FrameRenderTarget            &target,
                                                        const Matrix4                      &projection,
                                                        GraphImageHandle                    sceneDepth,
                                                        const SceneHighlightStyle          &style,
                                                        std::span<const ResolvedRasterDraw> selected)
    {
        if (selected.empty())
            return FrameOperationComplete{};
        if (!maskShader || !outlineShader)
            return FrameError{FrameErrorCode::INVALID_ARGUMENT, "Highlight programs were not prepared before graph construction"};
        Image::Descriptor desc;
        desc.extent      = {target.output.extent.width, target.output.extent.height, 1};
        desc.format      = PixelFormat::RGBA8_UNORM;
        desc.usage       = ImageUsageFlagBit::RENDER_TARGET | ImageUsageFlagBit::SAMPLED;
        const auto image = context.CreateImage(desc);
        if (const auto *error = std::get_if<FrameError>(&image))
            return *error;
        desc.format      = PixelFormat::D32;
        desc.usage       = ImageUsageFlagBit::DEPTH_STENCIL;
        const auto depth = context.CreateImage(desc);
        if (const auto *error = std::get_if<FrameError>(&depth))
            return *error;
        PassResourceBindings passBindings(shared::GetSelectionMaskPassReflection());
        auto result = passBindings.UniformData(shared::SelectionMaskPassBindings::gSelectionMask, shared::SelectionMaskParams{projection});
        if (const auto *error = std::get_if<FrameError>(&result))
            return *error;
        result = passBindings.Image(shared::SelectionMaskPassBindings::gSceneDepth, sceneDepth);
        if (const auto *error = std::get_if<FrameError>(&result))
            return *error;
        FrameRasterPass mask;
        mask.color = std::get<GraphImageHandle>(image);
        mask.clear = {0.f, 0.f, 0.f, 0.f};
        mask.depth = RasterDepthAttachment{.image = std::get<GraphImageHandle>(depth)};
        for (const auto &source : selected) {
            RasterDraw draw = source.draw;
            draw.shader     = maskShader;
            draw.blend      = {};
            draw.depth      = {.test = true, .write = true};
            auto retainedBindings = ResolveRasterBindings(draw);
            retainedBindings.uniforms.clear();
            retainedBindings.images.clear();
            retainedBindings.samplers.clear();
            result = SetRasterBindings(draw, std::move(retainedBindings));
            if (const auto *error = std::get_if<FrameError>(&result))
                return *error;
            const auto position = std::ranges::find_if(
                draw.attributes, [](const auto &a) { return a.semantic == "POSITION" && a.semanticIndex == 0 && a.format == Format::F_RGB32; });
            if (position == draw.attributes.end())
                return FrameError{FrameErrorCode::UNSUPPORTED, "Highlight requires float3 POSITION0"};
            auto attribute     = *position;
            attribute.location = 0;
            draw.attributes    = {std::move(attribute)};
            PassResourceBindings batch(shared::GetSceneBatchReflection());
            result = batch.UniformData(shared::SceneBatchBindings::gSceneBatch, shared::SceneBatchParams{source.world});
            if (const auto *error = std::get_if<FrameError>(&result))
                return *error;
            const PassResourceBindings *groups[]{&passBindings, &batch};
            result = PassResourceBindings::Apply(context, draw, groups);
            if (const auto *error = std::get_if<FrameError>(&result))
                return *error;
            mask.draws.push_back(std::move(draw));
        }
        result = context.Raster(std::move(mask));
        if (const auto *error = std::get_if<FrameError>(&result))
            return *error;
        PassResourceBindings                 outline(shared::GetSelectionOutlinePassReflection());
        const shared::SelectionOutlineParams params{
            style.visible, style.occluded, {float(target.output.extent.width), float(target.output.extent.height), style.width, 0}};
        result = outline.UniformData(shared::SelectionOutlinePassBindings::gSelectionOutline, params);
        if (const auto *error = std::get_if<FrameError>(&result))
            return *error;
        result = outline.Image(shared::SelectionOutlinePassBindings::gSelectionImage, std::get<GraphImageHandle>(image));
        if (const auto *error = std::get_if<FrameError>(&result))
            return *error;
        RasterDraw draw;
        draw.shader              = outlineShader;
        draw.scissor.extent      = target.output.extent;
        draw.command.vertexCount = 3;
        draw.blend               = {.blendEn  = true,
                                    .srcColor = BlendFactor::SRC_ALPHA,
                                    .dstColor = BlendFactor::ONE_MINUS_SRC_ALPHA,
                                    .srcAlpha = BlendFactor::ZERO,
                                    .dstAlpha = BlendFactor::ONE};
        const PassResourceBindings *groups[]{&outline};
        result = PassResourceBindings::Apply(context, draw, groups);
        if (const auto *error = std::get_if<FrameError>(&result))
            return *error;
        FrameRasterPass composite;
        composite.color = target.color;
        composite.load  = LoadOp::LOAD;
        composite.draws.push_back(std::move(draw));
        return context.Fullscreen(std::move(composite));
    }
    void SceneHighlightRenderer::Reset()
    {
        maskShader.Reset(nullptr);
        outlineShader.Reset(nullptr);
    }
} // namespace sky::aurora

//
// Created on 2026/10/20.
//

#include <aurora/pipeline/SceneRenderFeature.h>

#include "GlobalBlock.h"
#include "OpaquePass.h"
#include "SceneBatch.h"
#include "SceneEnvironmentPass.h"
#include "SceneHighlightRenderer.h"
#include "SceneLightingPass.h"
#include "SceneSurfaceBatch.h"
#include "ShadowPass.h"
#include "ToneMapPass.h"
#include <algorithm>
#include <aurora/renderer/PassResourceBindings.h>
#include <aurora/rhi/Device.h>
#include <aurora/shader/ShaderFileSystem.h>
#include <aurora/shader/ShaderTechnique.h>
#include <cmath>
#include <cstring>
#include <limits>
#include <set>
#include <tuple>

namespace sky::aurora {
    namespace {
        struct SceneInput {
            SceneViewSettings       settings;
            std::array<uint32_t, 3> images{};
        };
        template <typename T>
        FrameOperationResult WriteParams(FrameGraphBuildContext &context,
                                         PassResourceBindings   &bindings,
                                         ShaderUniformSlot<T>    slot,
                                         const T                &value,
                                         ShaderResourceType      type = ShaderResourceType::UNIFORM_BUFFER)
        {
            (void)context;
            return bindings.UniformData(slot, value, type);
        }

        FrameOperationResult BindGlobal(FrameGraphBuildContext &context, PassResourceBindings &bindings)
        {
            const auto                &frame = *context.GetInputs().prepared;
            const shared::GlobalParams params{{static_cast<float>(frame.timeSeconds), frame.deltaSeconds, 0, 0}};
            if (!std::isfinite(params.frameTime.x) || !std::isfinite(params.frameTime.y)) {
                return FrameError{.code = FrameErrorCode::INVALID_ARGUMENT, .message = "Frame time exceeds shader float range"};
            }
            return WriteParams(context, bindings, shared::GlobalBlockBindings::gGlobal, params);
        }

        std::variant<FrameError, ShaderReflection> MaterialSchema(const Shader &shader, const RenderMaterialParameters &parameters)
        {
            ShaderReflection                        result;
            std::set<std::pair<uint32_t, uint32_t>> keys;
            const auto                              add = [&](Name name) -> std::optional<FrameError> {
                ShaderParameterHandle handle;
                const auto            status = shader.GetParameterTable().Resolve(name, 2, handle);
                if (status != ShaderParameterLookup::READY && status != ShaderParameterLookup::INACTIVE)
                    return FrameError{FrameErrorCode::INVALID_ARGUMENT, "Material parameter is unknown or ambiguous"};
                const ShaderParameterEntry *entry = nullptr;
                shader.GetParameterTable().Resolve(handle, entry);
                if (!entry)
                    return FrameError{FrameErrorCode::INVALID_ARGUMENT, "Material parameter handle is stale"};
                if (!keys.emplace(entry->set, entry->binding).second)
                    return {};
                const auto  matches = [&](const auto &value) { return value.set == entry->set && value.binding == entry->binding; };
                const auto &reflection = shader.GetReflection();
                const auto  active     = std::find_if(reflection.resources.begin(), reflection.resources.end(), matches);
                const auto  declared = std::find_if(reflection.declarations.begin(), reflection.declarations.end(), matches);
                if (active != reflection.resources.end())
                    result.resources.push_back(*active);
                else if (declared != reflection.declarations.end())
                    result.resources.push_back(*declared);
                else
                    return FrameError{FrameErrorCode::INVALID_ARGUMENT, "Material parameter has no logical resource declaration"};
                const auto block = std::find_if(reflection.blocks.begin(), reflection.blocks.end(), matches);
                const auto declaration = std::find_if(reflection.declaredBlocks.begin(), reflection.declaredBlocks.end(), matches);
                if (block != reflection.blocks.end())
                    result.blocks.push_back(*block);
                else if (declaration != reflection.declaredBlocks.end())
                    result.blocks.push_back(*declaration);
                return {};
            };
            for (const auto &value : parameters.values)
                if (const auto error = add(value.name))
                    return *error;
            for (const auto &image : parameters.images)
                if (const auto error = add(image.name))
                    return *error;
            for (const auto &sampler : parameters.samplers)
                if (const auto error = add(sampler.name))
                    return *error;
            return result;
        }
        FrameOperationResult BindMaterial(FrameGraphBuildContext &context, const RenderMaterialParameters &parameters, PassResourceBindings &bindings)
        {
            for (const auto &value : parameters.values)
                if (const auto result = bindings.Data(value.name, value.value); std::holds_alternative<FrameError>(result))
                    return result;
            for (const auto &value : parameters.images) {
                const auto image = context.ResolveImage(value.resource);
                if (!image)
                    return FrameError{FrameErrorCode::INVALID_ARGUMENT, "Material image version is not available in this graph"};
                if (const auto result = bindings.Image(value.name, *image); std::holds_alternative<FrameError>(result))
                    return result;
            }
            for (const auto &value : parameters.samplers) {
                auto sampler = context.CreateSampler(value.descriptor);
                if (const auto *error = std::get_if<FrameError>(&sampler))
                    return *error;
                if (const auto result = bindings.Sampler(value.name, std::get<SamplerPtr>(std::move(sampler)));
                    std::holds_alternative<FrameError>(result))
                    return result;
            }
            return FrameOperationComplete{};
        }

        std::variant<FrameError, ShaderPtr>
        CreateSceneShader(FrameGraphBuildContext &context, ShaderFileSystem *files, IFileSystem *products, API backend, const char *path)
        {
            if (files == nullptr) {
                return FrameError{.code = FrameErrorCode::INVALID_ARGUMENT, .message = "Scene feature has no shader source provider"};
            }
            const ShaderTechnique technique{.vertex   = {.relativePath = path, .entry = "VSMain"},
                                            .fragment = {.relativePath = path, .entry = "FSMain"}};
            const auto            compiled = CompileShaderTechnique(*files, backend, technique, products);
            if (const auto *error = std::get_if<RendererError>(&compiled)) {
                return FrameError{.code = FrameErrorCode::COMPILE_FAILED, .message = error->message};
            }
            return context.CreateShader(std::get<RenderTechniqueData>(compiled));
        }

        std::variant<FrameError, Matrix4> ShadowProjection(const PreparedViewData &view, Vector3 forward)
        {
            forward.Normalize();
            Vector3 right = (std::abs(forward.y) > .95f ? Vector3{1, 0, 0} : Vector3{0, 1, 0}).Cross(forward);
            right.Normalize();
            const auto up = forward.Cross(right);
            Vector3    low{std::numeric_limits<float>::max()};
            Vector3    high{-std::numeric_limits<float>::max()};
            bool       any = false;
            for (const auto *draws : {&view.draws, &view.shadowCasters}) {
                for (const auto &draw : *draws) {
                    if (!draw.worldBounds) {
                        return FrameError{.code = FrameErrorCode::UNSUPPORTED, .message = "Directional shadows require mesh bounds"};
                    }
                    const auto &bounds = *draw.worldBounds;
                    for (uint32_t corner = 0; corner < 8; ++corner) {
                        const Vector3 p{((corner & 1) != 0u) ? bounds.max.x : bounds.min.x, ((corner & 2) != 0u) ? bounds.max.y : bounds.min.y,
                                        ((corner & 4) != 0u) ? bounds.max.z : bounds.min.z};
                        const Vector3 projected{right.Dot(p), up.Dot(p), forward.Dot(p)};
                        for (uint32_t axis = 0; axis < 3; ++axis) {
                            low[axis]  = std::min(low[axis], projected[axis]);
                            high[axis] = std::max(high[axis], projected[axis]);
                        }
                        any = true;
                    }
                }
            }
            if (!any) {
                return RenderIdentityMatrix();
            }
            low -= Vector3{.05f};
            high += Vector3{.05f};
            const auto size = high - low;
            Matrix4    result{};
            for (uint32_t axis = 0; axis < 3; ++axis) {
                result.v[axis * 4]       = right[axis] * 2 / size.x;
                result.v[(axis * 4) + 1] = up[axis] * 2 / size.y;
                result.v[(axis * 4) + 2] = forward[axis] / size.z;
                result.v[(axis * 4) + 3] = 0;
            }
            result.v[12] = -(high.x + low.x) / size.x;
            result.v[13] = -(high.y + low.y) / size.y;
            result.v[14] = -low.z / size.z;
            result.v[15] = 1;
            if (!std::ranges::all_of(result.v, [](float v) { return std::isfinite(v); })) {
                return FrameError{.code = FrameErrorCode::INVALID_ARGUMENT, .message = "Shadow projection exceeds the finite coordinate range"};
            }
            return result;
        }

        class SceneRenderFeature final : public IFrameGraphFeature {
        public:
            SceneRenderFeature(Name name, std::shared_ptr<ShaderFileSystem> sources, FileSystemPtr products)
                : name(name), sources(sources ? std::move(sources) : std::make_shared<ShaderFileSystem>()), products(std::move(products))
            {
            }
            Name GetName() const override
            {
                return name;
            }
            std::optional<RendererError> Validate(const FrameExtensionPayload &payload) const override
            {
                if (IsSceneHighlightInput(payload))
                    return ValidateSceneHighlightInput(payload);
                if (payload.outputs.size() != 1 || payload.resources.size() > 3 || payload.bytes.size() != sizeof(SceneInput)) {
                    return RendererError{.code = RendererErrorCode::INVALID_ARGUMENT, .message = "Scene view settings require one output"};
                }
                SceneInput input;
                std::memcpy(&input, payload.bytes.data(), sizeof(input));
                const auto &settings = input.settings;
                if (settings.hdr > 1 || !std::isfinite(settings.exposure) || settings.exposure < -16 || settings.exposure > 16) {
                    return RendererError{.code    = RendererErrorCode::INVALID_ARGUMENT,
                                         .message = "Scene exposure must be finite and between -16 and 16 EV"};
                }
                if (!std::isfinite(settings.environmentIntensity) || settings.environmentIntensity < 0 || settings.environmentIntensity > 64) {
                    return RendererError{.code    = RendererErrorCode::INVALID_ARGUMENT,
                                         .message = "Environment intensity must be finite and between 0 and 64"};
                }
                uint32_t used = 0;
                for (const auto index : input.images) {
                    if (index > payload.resources.size()) {
                        return RendererError{.code = RendererErrorCode::INVALID_ARGUMENT, .message = "Environment image index is invalid"};
                    }
                    if (index != 0u) {
                        used |= 1u << (index - 1);
                    }
                }
                if (used != (1u << payload.resources.size()) - 1) {
                    return RendererError{.code = RendererErrorCode::INVALID_ARGUMENT, .message = "Environment input has unused resource versions"};
                }
                return {};
            }
            FrameOperationResult BuildFrame(FrameGraphBuildContext &, const FrameExtensionPayload &) override
            {
                return FrameOperationComplete{};
            }
            FrameOperationResult BuildView(FrameGraphBuildContext &context, const PreparedViewData &view) override
            {
                const auto target = context.GetOutput(view.view.output);
                if (!target) {
                    return FrameError{.code = FrameErrorCode::INVALID_ARGUMENT, .message = "Scene pass requires its view output"};
                }
                SceneViewSettings      settings;
                SceneEnvironmentInputs environmentImages;
                if (const auto error = ReadViewSettings(context, view.view.output, settings, environmentImages)) {
                    return *error;
                }
                auto resolved = context.ResolveDraws(Name("opaque"), {.offset = {.x = 0, .y = 0}, .extent = target->output.extent});
                if (const auto *error = std::get_if<FrameError>(&resolved)) {
                    return *error;
                }
                FrameRasterPass pass;
                pass.color = target->color;
                pass.load  = target->initialized ? LoadOp::LOAD : LoadOp::CLEAR;
                // Scene projections are Y-up; output mapping consumes top-left logical coordinates.
                auto sceneToLogical = RenderIdentityMatrix();
                sceneToLogical.v[5] = -1;
                const shared::OpaquePassParams params{
                    .viewProjection = target->logicalToClip * sceneToLogical * view.view.projection * view.view.view,
                    .cameraPosition = {view.view.cameraPosition.x, view.view.cameraPosition.y, view.view.cameraPosition.z, 1}};
                auto      &draws          = std::get<std::vector<ResolvedRasterDraw>>(resolved);
                const auto highlightInput = ReadSceneHighlightInput(context, name, view.view.output);
                if (const auto *error = std::get_if<FrameError>(&highlightInput))
                    return *error;
                const auto                     &highlight = std::get<SceneHighlightData>(highlightInput);
                std::vector<ResolvedRasterDraw> selected;
                if (!highlight.entities.empty()) {
                    const auto key = [](const RenderEntityRef &id) {
                        return std::tuple{id.scene.index, id.scene.generation, id.entity.index, id.entity.generation};
                    };
                    std::set<decltype(key(RenderEntityRef{}))> selectedIds;
                    for (const auto &id : highlight.entities)
                        selectedIds.insert(key(id));
                    for (const auto &draw : draws)
                        if (selectedIds.contains(key(draw.entity)))
                            selected.push_back(draw);
                }
                const bool hdr     = (settings.hdr != 0u) && !draws.empty();
                const auto backend = draws.empty() ? API::VULKAN : draws.front().draw.shader->GetDevice()->GetAPI();
                if (hdr) {
                    if (target->initialized) {
                        return FrameError{.code    = FrameErrorCode::UNSUPPORTED,
                                          .message = "HDR scene output requires exclusive scene ownership before compositing"};
                    }
                    Image::Descriptor color;
                    color.format     = PixelFormat::RGBA16_SFLOAT;
                    color.extent     = {.width = target->output.extent.width, .height = target->output.extent.height, .depth = 1};
                    color.usage      = ImageUsageFlagBit::RENDER_TARGET | ImageUsageFlagBit::SAMPLED;
                    const auto image = context.CreateImage(color);
                    if (const auto *error = std::get_if<FrameError>(&image)) {
                        return *error;
                    }
                    pass.color = std::get<GraphImageHandle>(image);
                }
                PassResourceBindings lighting(shared::GetSceneLightingPassReflection());
                PassResourceBindings environment(shared::GetSceneEnvironmentPassReflection());
                const auto           lit = std::ranges::find_if(draws, [](const auto &draw) {
                    const auto &resources = draw.draw.shader->GetReflection().resources;
                    return std::any_of(resources.begin(), resources.end(), [](const auto &resource) {
                        constexpr auto slot = shared::SceneLightingPassBindings::gSceneLighting;
                        return resource.set == slot.set && resource.binding == slot.binding;
                    });
                });
                if (lit != draws.end()) {
                    const auto result = BuildLighting(context, view, lit->draw.shader->GetDevice()->GetAPI(), lighting);
                    if (std::holds_alternative<FrameError>(result)) {
                        return result;
                    }
                    const auto bound = BuildEnvironment(context, environmentImages, settings.environmentIntensity, environment);
                    if (std::holds_alternative<FrameError>(bound)) {
                        return bound;
                    }
                }
                auto bound = BindDraws(context, params, draws, lighting, environment, pass.draws);
                if (std::holds_alternative<FrameError>(bound)) {
                    return bound;
                }
                if (!selected.empty() || std::ranges::any_of(pass.draws, [](const auto &draw) { return draw.depth.test || draw.depth.write; })) {
                    Image::Descriptor depth;
                    depth.format = PixelFormat::D32;
                    depth.extent = {.width = target->output.extent.width, .height = target->output.extent.height, .depth = 1};
                    depth.usage  = ImageUsageFlagBit::DEPTH_STENCIL;
                    if (!selected.empty())
                        depth.usage |= ImageUsageFlagBit::SAMPLED;
                    const auto image = context.CreateImage(depth);
                    if (const auto *error = std::get_if<FrameError>(&image)) {
                        return *error;
                    }
                    pass.depth = RasterDepthAttachment{.image = std::get<GraphImageHandle>(image)};
                }
                const auto color      = pass.color;
                const auto sceneDepth = pass.depth ? pass.depth->image : GraphImageHandle{};
                auto       rendered   = context.Raster(std::move(pass));
                if (std::holds_alternative<FrameError>(rendered)) {
                    return rendered;
                }
                if (hdr) {
                    rendered = ToneMap(context, *target, color, settings.exposure, backend);
                    if (std::holds_alternative<FrameError>(rendered))
                        return rendered;
                }
                return highlights.Render(context, *target, params.viewProjection, sceneDepth, highlight.style, selected, *sources, products.Get());
            }

            void OnRetired() override
            {
                highlights.Reset();
                shadowSampler.Reset(nullptr);
                toneMapShader.Reset(nullptr);
                toneMapSampler.Reset(nullptr);
                environmentSampler.Reset(nullptr);
                brdfSampler.Reset(nullptr);
            }

        private:
            std::optional<FrameError> ReadViewSettings(const FrameGraphBuildContext &context,
                                                       ViewportId                    output,
                                                       SceneViewSettings            &settings,
                                                       SceneEnvironmentInputs       &environmentImages) const
            {
                bool settingsFound = false;
                for (const auto &input : context.GetInputs().prepared->extensions) {
                    if (input.feature != name) {
                        continue;
                    }
                    if (IsSceneHighlightInput(input))
                        continue;
                    if (const auto error = Validate(input)) {
                        return FrameError{.code = FrameErrorCode::INVALID_ARGUMENT, .message = error->message};
                    }
                    if (input.outputs.front() != output) {
                        continue;
                    }
                    if (settingsFound) {
                        return FrameError{.code = FrameErrorCode::INVALID_ARGUMENT, .message = "Duplicate scene view settings"};
                    }
                    SceneInput sceneInput;
                    std::memcpy(&sceneInput, input.bytes.data(), sizeof(sceneInput));
                    settings = sceneInput.settings;
                    for (size_t i = 0; i < environmentImages.size(); ++i) {
                        if (sceneInput.images[i] != 0u) {
                            environmentImages[i] = input.resources[sceneInput.images[i] - 1];
                        }
                    }
                    settingsFound = true;
                }
                return {};
            }

            static FrameOperationResult BindDraws(FrameGraphBuildContext         &context,
                                                  const shared::OpaquePassParams &params,
                                                  std::span<ResolvedRasterDraw>   draws,
                                                  const PassResourceBindings     &lighting,
                                                  const PassResourceBindings     &environment,
                                                  std::vector<RasterDraw>        &boundDraws)
            {
                struct PassBindings {
                    PassResourceBindings bindings{shared::GetOpaquePassReflection()};
                    bool                 ready = false;
                };
                std::array<PassBindings, 2> passes;
                PassResourceBindings        global(shared::GetGlobalBlockReflection());
                if (const auto result = BindGlobal(context, global); std::holds_alternative<FrameError>(result)) {
                    return result;
                }
                boundDraws.reserve(draws.size());
                for (auto &resolvedDraw : draws) {
                    auto                          &draw       = resolvedDraw.draw;
                    const auto                    &reflection = draw.shader->GetReflection();
                    const shared::SceneBatchParams batch{.world = resolvedDraw.world};
                    const auto                     find = [&](auto slot) {
                        return std::find_if(reflection.resources.begin(), reflection.resources.end(),
                                                                [&](const auto &resource) { return resource.set == slot.set && resource.binding == slot.binding; });
                    };
                    const auto           batchResource = find(shared::SceneBatchBindings::gSceneBatch);
                    PassResourceBindings batchBindings(shared::GetSceneBatchReflection());
                    if (batchResource != reflection.resources.end()) {
                        if (const auto result =
                                WriteParams(context, batchBindings, shared::SceneBatchBindings::gSceneBatch, batch, batchResource->type);
                            std::holds_alternative<FrameError>(result)) {
                            return result;
                        }
                    }
                    const auto materialSchema = MaterialSchema(*draw.shader, resolvedDraw.material);
                    if (const auto *error = std::get_if<FrameError>(&materialSchema))
                        return *error;
                    PassResourceBindings material(std::get<ShaderReflection>(materialSchema));
                    if (const auto result = BindMaterial(context, resolvedDraw.material, material); std::holds_alternative<FrameError>(result)) {
                        return result;
                    }
                    const auto passResource = find(shared::OpaquePassBindings::gOpaquePass);
                    const auto passIndex    = static_cast<size_t>(passResource != reflection.resources.end() &&
                                                                  passResource->type == ShaderResourceType::UNIFORM_BUFFER_DYNAMIC);
                    auto      &passBindings = passes[passIndex].bindings;
                    auto      &ready        = passes[passIndex].ready;
                    if (passResource != reflection.resources.end() && !ready) {
                        const auto result = WriteParams(context, passBindings, shared::OpaquePassBindings::gOpaquePass, params, passResource->type);
                        if (std::holds_alternative<FrameError>(result)) {
                            return result;
                        }
                        ready = true;
                    }
                    PassResourceBindings surface(shared::GetSceneSurfaceBatchReflection());
                    const auto           surfaceResource = find(shared::SceneSurfaceBatchBindings::gSceneSurface);
                    if (surfaceResource != reflection.resources.end()) {
                        const shared::SceneSurfaceParams surfaceParams{{static_cast<float>(resolvedDraw.receiveShadow), 0, 0, 0}};
                        const auto                       result =
                            WriteParams(context, surface, shared::SceneSurfaceBatchBindings::gSceneSurface, surfaceParams, surfaceResource->type);
                        if (std::holds_alternative<FrameError>(result)) {
                            return result;
                        }
                    }
                    const PassResourceBindings *groups[]{&global, &passBindings, &batchBindings, &material, &lighting, &surface, &environment};
                    if (const auto  result = PassResourceBindings::Apply(context, draw, groups);
                        const auto *error  = std::get_if<FrameError>(&result)) {
                        return FrameError{.code = error->code, .message = "Shader does not match the scene pass declarations: " + error->message};
                    }
                    boundDraws.push_back(std::move(draw));
                }
                return FrameOperationComplete{};
            }

            FrameOperationResult
            BuildEnvironment(FrameGraphBuildContext &context, const SceneEnvironmentInputs &inputs, float intensity, PassResourceBindings &bindings)
            {
                for (auto *sampler : {&environmentSampler, &brdfSampler}) {
                    if (*sampler) {
                        continue;
                    }
                    Sampler::Descriptor desc;
                    desc.addressModeV = desc.addressModeW = WrapMode::CLAMP_TO_EDGE;
                    desc.addressModeU                     = sampler == &environmentSampler ? WrapMode::REPEAT : WrapMode::CLAMP_TO_EDGE;
                    const auto created                    = context.CreateSampler(desc);
                    if (const auto *error = std::get_if<FrameError>(&created)) {
                        return *error;
                    }
                    *sampler = std::get<SamplerPtr>(created);
                }
                using Slots      = shared::SceneEnvironmentPassBindings;
                const auto slots = std::array{Slots::gDiffuseIrradiance, Slots::gSpecularEnvironment, Slots::gBrdfIntegration};
                for (size_t i = 0; i < inputs.size(); ++i) {
                    auto image = inputs[i] ? context.ResolveImage(*inputs[i]) : std::optional<GraphImageHandle>{};
                    if (!image) {
                        Image::Descriptor desc;
                        desc.format        = PixelFormat::RGBA16_SFLOAT;
                        desc.extent        = {.width = 1, .height = 1, .depth = 1};
                        desc.usage         = ImageUsageFlagBit::RENDER_TARGET | ImageUsageFlagBit::SAMPLED;
                        const auto created = context.CreateImage(desc);
                        if (const auto *error = std::get_if<FrameError>(&created)) {
                            return *error;
                        }
                        image = std::get<GraphImageHandle>(created);
                        FrameRasterPass clear;
                        clear.color       = *image;
                        const float value = i == 0 ? .03f : 0.f;
                        clear.clear       = {i == 2 ? 1.f : value, value, value, 1.f};
                        const auto result = context.Raster(std::move(clear));
                        if (std::holds_alternative<FrameError>(result)) {
                            return result;
                        }
                    }
                    const auto bound = bindings.Image(slots[i], *image);
                    if (std::holds_alternative<FrameError>(bound)) {
                        return bound;
                    }
                }
                auto result = WriteParams(context, bindings, Slots::gSceneEnvironment, shared::SceneEnvironmentParams{{intensity, 0, 0, 0}});
                if (std::holds_alternative<FrameError>(result)) {
                    return result;
                }
                result = bindings.Sampler(Slots::gEnvironmentSampler, environmentSampler);
                if (std::holds_alternative<FrameError>(result)) {
                    return result;
                }
                return bindings.Sampler(Slots::gBrdfSampler, brdfSampler);
            }
            FrameOperationResult
            ToneMap(FrameGraphBuildContext &context, const FrameRenderTarget &target, GraphImageHandle color, float exposure, API backend)
            {
                if (!toneMapShader) {
                    const auto created = CreateSceneShader(context, sources.get(), products.Get(), backend, "aurora/pipeline/ToneMap.slang");
                    if (const auto *error = std::get_if<FrameError>(&created)) {
                        return *error;
                    }
                    toneMapShader = std::get<ShaderPtr>(created);
                }
                if (!toneMapSampler) {
                    Sampler::Descriptor desc;
                    desc.minFilter = desc.magFilter = Filter::NEAREST;
                    desc.addressModeU = desc.addressModeV = desc.addressModeW = WrapMode::CLAMP_TO_EDGE;
                    const auto created                                        = context.CreateSampler(desc);
                    if (const auto *error = std::get_if<FrameError>(&created)) {
                        return *error;
                    }
                    toneMapSampler = std::get<SamplerPtr>(created);
                }
                const auto                  format = target.output.format;
                const bool                  srgb   = format == PixelFormat::RGBA8_SRGB || format == PixelFormat::BGRA8_SRGB;
                const shared::ToneMapParams params{{1.f / static_cast<float>(target.output.extent.width),
                                                    1.f / static_cast<float>(target.output.extent.height), exposure, srgb ? 0.f : 1.f}};
                PassResourceBindings        bindings(shared::GetToneMapPassReflection());
                auto                        result = WriteParams(context, bindings, shared::ToneMapPassBindings::gToneMap, params);
                if (std::holds_alternative<FrameError>(result)) {
                    return result;
                }
                result = bindings.Image(shared::ToneMapPassBindings::gHdrColor, color);
                if (std::holds_alternative<FrameError>(result)) {
                    return result;
                }
                result = bindings.Sampler(shared::ToneMapPassBindings::gHdrSampler, toneMapSampler);
                if (std::holds_alternative<FrameError>(result)) {
                    return result;
                }
                RasterDraw draw;
                draw.shader              = toneMapShader;
                draw.command.vertexCount = 3;
                draw.scissor.extent      = target.output.extent;
                const PassResourceBindings *groups[]{&bindings};
                result = PassResourceBindings::Apply(context, draw, groups);
                if (std::holds_alternative<FrameError>(result)) {
                    return result;
                }
                FrameRasterPass pass;
                pass.color = target.color;
                pass.draws.push_back(std::move(draw));
                return context.Fullscreen(std::move(pass));
            }
            static FrameOperationResult BindShadowDraws(FrameGraphBuildContext       &context,
                                                        std::span<ResolvedRasterDraw> casters,
                                                        const Matrix4                &projection,
                                                        std::vector<RasterDraw>      &draws)
            {
                PassResourceBindings pass(shared::GetShadowPassReflection());
                PassResourceBindings global(shared::GetGlobalBlockReflection());
                if (const auto result = BindGlobal(context, global); std::holds_alternative<FrameError>(result)) {
                    return result;
                }
                const auto written = WriteParams(context, pass, shared::ShadowPassBindings::gShadowPass, shared::ShadowPassParams{projection});
                if (std::holds_alternative<FrameError>(written)) {
                    return written;
                }
                for (auto &caster : casters) {
                    auto                &draw = caster.draw;
                    PassResourceBindings batch(shared::GetSceneBatchReflection());
                    const auto           result =
                        WriteParams(context, batch, shared::SceneBatchBindings::gSceneBatch, shared::SceneBatchParams{.world = caster.world});
                    if (std::holds_alternative<FrameError>(result)) {
                        return result;
                    }
                    const auto materialSchema = MaterialSchema(*draw.shader, caster.material);
                    if (const auto *error = std::get_if<FrameError>(&materialSchema))
                        return *error;
                    PassResourceBindings material(std::get<ShaderReflection>(materialSchema));
                    if (const auto result = BindMaterial(context, caster.material, material); std::holds_alternative<FrameError>(result)) {
                        return result;
                    }
                    const PassResourceBindings *groups[]{&global, &pass, &batch, &material};
                    const auto                  applied = PassResourceBindings::Apply(context, draw, groups);
                    if (std::holds_alternative<FrameError>(applied)) {
                        return applied;
                    }
                    draws.push_back(std::move(draw));
                }
                return FrameOperationComplete{};
            }
            FrameOperationResult
            BuildLighting(FrameGraphBuildContext &context, const PreparedViewData &view, API backend, PassResourceBindings &bindings)
            {
                shared::SceneLightingParams params{};
                if (view.lights.size() > std::size(params.positions)) {
                    return FrameError{.code = FrameErrorCode::UNSUPPORTED, .message = "Scene lighting supports at most eight active lights per view"};
                }
                params.lightCount.x         = static_cast<float>(view.lights.size());
                params.shadowViewProjection = RenderIdentityMatrix();
                params.shadowControl        = {0, .001f, backend == API::VULKAN ? 1.f : -1.f, 0};
                for (size_t i = 0; i < view.lights.size(); ++i) {
                    const auto &source    = view.lights[i];
                    const auto &light     = source.light;
                    auto        direction = light.direction;
                    if (light.kind != RenderLightKind::POINT) {
                        const double length = std::sqrt((double(direction.x) * direction.x) + (double(direction.y) * direction.y) +
                                                        (double(direction.z) * direction.z));
                        if (!std::isfinite(length) || length < 1e-6) {
                            return FrameError{.code    = FrameErrorCode::INVALID_ARGUMENT,
                                              .message = "Directional and spot lights require a nonzero finite direction"};
                        }
                        direction = {static_cast<float>(direction.x / length), static_cast<float>(direction.y / length),
                                     static_cast<float>(direction.z / length)};
                    }
                    if (!std::isfinite(light.color.x * light.intensity) || !std::isfinite(light.color.y * light.intensity) ||
                        !std::isfinite(light.color.z * light.intensity)) {
                        return FrameError{.code = FrameErrorCode::INVALID_ARGUMENT, .message = "Light radiance exceeds the finite range"};
                    }
                    params.positions[i]  = {source.position.x, source.position.y, source.position.z, light.range};
                    params.directions[i] = {direction.x, direction.y, direction.z, static_cast<float>(light.kind)};
                    params.colors[i]     = {light.color.x, light.color.y, light.color.z, light.intensity};
                    params.cones[i]      = {std::cos(light.innerConeAngle), std::cos(light.outerConeAngle), 0, 0};
                    if ((params.shadowControl.x == 0.0f) && light.kind == RenderLightKind::DIRECTIONAL && light.castShadow &&
                        !view.shadowCasters.empty()) {
                        const auto projected = ShadowProjection(view, direction);
                        if (const auto *error = std::get_if<FrameError>(&projected)) {
                            return *error;
                        }
                        params.shadowControl.x      = static_cast<float>(i + 1);
                        params.shadowViewProjection = std::get<Matrix4>(projected);
                    }
                }
                const uint32_t    resolution = (params.shadowControl.x != 0.0f) ? 512 : 1;
                Image::Descriptor desc;
                desc.format      = PixelFormat::D32;
                desc.extent      = {.width = resolution, .height = resolution, .depth = 1};
                desc.usage       = ImageUsageFlagBit::DEPTH_STENCIL | ImageUsageFlagBit::SAMPLED;
                const auto image = context.CreateImage(desc);
                if (const auto *error = std::get_if<FrameError>(&image)) {
                    return *error;
                }
                FrameRasterPass shadow;
                shadow.depth = RasterDepthAttachment{.image = std::get<GraphImageHandle>(image)};
                if (params.shadowControl.x != 0.0f) {
                    auto resolved =
                        context.ResolveDraws(Name("shadow"), {.offset = {.x = 0, .y = 0}, .extent = {.width = resolution, .height = resolution}},
                                             PreparedDrawList::SHADOW_CASTERS);
                    if (const auto *error = std::get_if<FrameError>(&resolved)) {
                        return *error;
                    }
                    if (const auto result =
                            BindShadowDraws(context, std::get<std::vector<ResolvedRasterDraw>>(resolved), params.shadowViewProjection, shadow.draws);
                        std::holds_alternative<FrameError>(result)) {
                        return result;
                    }
                }
                const auto raster = context.Raster(std::move(shadow));
                if (std::holds_alternative<FrameError>(raster)) {
                    return raster;
                }
                if (!shadowSampler) {
                    Sampler::Descriptor sampler;
                    sampler.minFilter = sampler.magFilter = Filter::NEAREST;
                    sampler.mipmapMode                    = MipFilter::NEAREST;
                    const auto created                    = context.CreateSampler(sampler);
                    if (const auto *error = std::get_if<FrameError>(&created)) {
                        return *error;
                    }
                    shadowSampler = std::get<SamplerPtr>(created);
                }
                const auto written = WriteParams(context, bindings, shared::SceneLightingPassBindings::gSceneLighting, params);
                if (std::holds_alternative<FrameError>(written)) {
                    return written;
                }
                const auto bound = bindings.Image(shared::SceneLightingPassBindings::gShadowImage, std::get<GraphImageHandle>(image));
                if (std::holds_alternative<FrameError>(bound)) {
                    return bound;
                }
                return bindings.Sampler(shared::SceneLightingPassBindings::gShadowSampler, shadowSampler);
            }
            Name                              name;
            std::shared_ptr<ShaderFileSystem> sources;
            FileSystemPtr                     products;
            SamplerPtr                        shadowSampler;
            ShaderPtr                         toneMapShader;
            SamplerPtr                        toneMapSampler;
            SamplerPtr                        environmentSampler, brdfSampler;
            SceneHighlightRenderer            highlights;
        };
    } // namespace

    std::shared_ptr<IFrameGraphFeature> CreateSceneRenderFeature(Name name, std::shared_ptr<ShaderFileSystem> sources, FileSystemPtr products)
    {
        return std::make_shared<SceneRenderFeature>(name, std::move(sources), std::move(products));
    }
    FrameExtensionPayload
    SceneViewInput(Name feature, ViewportId output, const SceneViewSettings &settings, const SceneEnvironmentInputs &environment)
    {
        FrameExtensionPayload payload;
        payload.feature = feature;
        payload.outputs = {output};
        SceneInput input{.settings = settings};
        for (size_t i = 0; i < environment.size(); ++i) {
            if (environment[i]) {
                payload.resources.push_back(*environment[i]);
                input.images[i] = static_cast<uint32_t>(payload.resources.size());
            }
        }
        payload.bytes.resize(sizeof(input));
        std::memcpy(payload.bytes.data(), &input, sizeof(input));
        return payload;
    }
} // namespace sky::aurora

//
// Created on 2026/10/17.
//
#pragma once
#include "MaterialBlock.h"
#include "OpaquePass.h"
#include "SceneBatch.h"
#include "ShaderSourcesFixture.h"
#include <aurora/renderer/RenderCommands.h>
#include <aurora/shader/ShaderCompilerSlang.h>
#include <aurora/shader/ShaderFileSystem.h>
#include <aurora/shader/ShaderTechnique.h>
#include <cstring>
#include <gtest/gtest.h>
namespace sky::aurora {
    inline RenderResourcePayload HostMesh(RendererInfo info)
    {
        RenderResourcePayload payload{
            RenderResourceKey{Uuid::CreateWithSeed(1201), 1, 1, info.deviceEpoch}, Name("runtime-mesh"), std::vector<uint8_t>(64), {}};
        const float    positions[] = {-0.8f, -0.8f, 0, -0.8f, 0.8f, 0, 0.8f, -0.8f, 0, 0.8f, 0.8f, 0};
        const uint16_t indices[]   = {0, 1, 2, 2, 1, 3};
        std::memcpy(payload.bytes.data(), positions, sizeof(positions));
        std::memcpy(payload.bytes.data() + 48, indices, sizeof(indices));
        payload.metadata.bounds = AABB{{-.8f, -.8f, 0}, {.8f, .8f, 0}};
        payload.mesh            = RenderMeshData{{{0, 48, 12}}, {{0, 0, 0, Format::F_RGB32, "POSITION", 0}}, {{48, 12, IndexType::U16, {6}}}};
        return payload;
    }
    inline constexpr const char *HostShaderAttributes = R"(
#ifndef AURORA_DECLARATION_ATTRIBUTES
#define AURORA_DECLARATION_ATTRIBUTES
[__AttributeUsage(_AttributeTargets.Var)]
struct AuroraResourceAttribute { int logicalSet; int logicalBinding; string owner; };
[__AttributeUsage(_AttributeTargets.Var)]
struct AuroraFieldAttribute { string path; };
#endif
)";

    inline RenderTechniqueData CompileHostRaster(RendererInfo info, const std::string &source)
    {
        RenderTechniqueData material;
        material.backend = info.backend;
        ShaderCompilerSlang compiler;
        ShaderFileSystem    files;
        if (!LoadTestShaderSources(files))
            return material;
        for (const bool vertex : {true, false}) {
            ShaderCompileDesc desc{};
            desc.source     = std::string(HostShaderAttributes) + source;
            desc.sourcePath = "fixture/HostRaster.slang";
            desc.fileSystem = &files;
            desc.entry      = vertex ? "VSMain" : "FSMain";
            desc.stage      = vertex ? ShaderStageFlagBit::VS : ShaderStageFlagBit::FS;
            desc.target     = info.backend == API::DX12 ? ShaderTarget::DXIL : info.backend == API::METAL ? ShaderTarget::MSL : ShaderTarget::SPIRV;
            ShaderCompileResult compiled;
            const bool success = compiler.Compile(desc, compiled);
            EXPECT_TRUE(success) << compiled.errorInfo;
            if (!success)
                return {};
            auto &artifact = vertex ? material.vertex : material.fragment;
            artifact       = std::make_shared<const ShaderStageArtifact>(std::move(static_cast<ShaderStageArtifact &>(compiled)));
            std::string error;
            const bool  merged = MergeShaderReflections(artifact->reflection, material.reflection, error);
            EXPECT_TRUE(merged) << error;
            if (!merged)
                return {};
            if (vertex)
                material.reflection.vertexInputs = artifact->reflection.vertexInputs;
        }
        return material;
    }
    inline RenderResourcePayload HostMaterial(RendererInfo info, bool withView = true, bool dynamicPass = false, bool dynamicBatch = true)
    {
        RenderResourcePayload payload;
        payload.resource         = {Uuid::CreateWithSeed(1202), 1, 1, info.deviceEpoch};
        payload.format           = Name("runtime-material");
        const std::string source = std::string("#include <aurora/pipeline/SceneBatch.slangh>\n#include <aurora/pipeline/MaterialBlock.slangh>\n") +
                                   (withView ? "#include <aurora/pipeline/OpaquePass.slangh>\n" : "") +
                                   "float4 VSMain(float3 position : POSITION) : SV_Position { return " +
                                   (withView ? "mul(gOpaquePass.viewProjection, mul(gSceneBatch.world, float4(position, 1)))"
                                             : "mul(gSceneBatch.world, float4(position, 1))") +
                                   "; }\nfloat4 FSMain() : SV_Target { return gMaterial.tint; }";
        auto &technique = payload.material.emplace().techniques.emplace_back();
        technique.tag   = Name("opaque");
        auto &material  = technique.program;
        material        = CompileHostRaster(info, source);
        material.parameterAliases = {{"tint", "gMaterial.tint"}, {"surface", "gMaterial.surface"}};
        const shared::MaterialBlockParams tint{{1, 0, 0, 1}, {.5f, 0, 0, 0}};
        technique.parameters.set    = shared::MaterialBlockBindings::gMaterial.set;
        technique.parameters.values = {{Name("tint"), MakeShaderData(tint.tint)},
                                       {Name("surface"), MakeShaderData(tint.surface)}};
        for (auto &resource : material.reflection.resources) {
            const bool dynamic = resource.set == shared::GetOpaquePassParamsLayout().set
                                     ? dynamicPass
                                     : (resource.set == shared::GetSceneBatchParamsLayout().set && dynamicBatch);
            if (dynamic && resource.type == ShaderResourceType::UNIFORM_BUFFER) {
                material.bindingPolicies.push_back({resource.set, resource.binding, ShaderResourceType::UNIFORM_BUFFER_DYNAMIC});
                resource.type = ShaderResourceType::UNIFORM_BUFFER_DYNAMIC;
            }
        }
        return payload;
    }

    inline RenderMaterialData TestSceneMaterial(RenderTechniqueData program, const Vector4 &tint)
    {
        RenderMaterialData material;
        auto              &technique = material.techniques.emplace_back();
        technique.tag                = Name("opaque");
        technique.program            = std::move(program);
        technique.program.parameterAliases = {{"tint", "gMaterial.tint"}, {"surface", "gMaterial.surface"}};
        technique.parameters.set          = shared::MaterialBlockBindings::gMaterial.set;
        technique.parameters.values  = {{Name("tint"), MakeShaderData(tint)},
                                        {Name("surface"), MakeShaderData(Vector4{.5f, 0, 0, 0})}};
        return material;
    }

    inline void AddHostMesh(FramePacket &packet, SceneId scene, RendererInfo info)
    {
        auto             mesh = HostMesh(info), material = HostMaterial(info);
        RenderEntityDesc entity;
        entity.mesh     = {mesh.resource, 1};
        entity.material = {material.resource, 1};
        packet.commands.push_back(CreateRenderEntity{{scene, {0, 1}}, entity});
        packet.commands.push_back(ResourceCpuReady{std::move(mesh)});
        packet.commands.push_back(ResourceCpuReady{std::move(material)});
    }
} // namespace sky::aurora

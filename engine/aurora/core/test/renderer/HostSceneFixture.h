//
// Created on 2026/10/17.
//
#pragma once
#include "SceneBatch.h"
#include "ScenePass.h"
#include <aurora/renderer/RenderCommands.h>
#include <aurora/shader/ShaderCompilerSlang.h>
#include <aurora/shader/ShaderFileSystem.h>
#include <cstring>
#include <gtest/gtest.h>
namespace sky::aurora {
    inline RenderResourcePayload HostMesh(RendererInfo info)
    {
        RenderResourcePayload payload{
            RenderResourceKey{Uuid::CreateWithSeed(1201), 1, 1, info.deviceEpoch}, Name("runtime-mesh"), std::vector<uint8_t>(64), {}};
        const float    positions[] = {-0.8f, -0.8f, -0.8f, 0.8f, 0.8f, -0.8f, 0.8f, 0.8f};
        const uint16_t indices[]   = {0, 1, 2, 2, 1, 3};
        std::memcpy(payload.bytes.data(), positions, sizeof(positions));
        std::memcpy(payload.bytes.data() + 32, indices, sizeof(indices));
        payload.metadata.bounds = AABB{{-.8f, -.8f, 0}, {.8f, .8f, 0}};
        payload.mesh            = RenderMeshData{{{0, 32, 8}}, {{0, 0, 0, Format::F_RG32, "POSITION", 0}}, {{32, 12, IndexType::U16, {6}}}};
        return payload;
    }
    inline RenderMaterialData CompileHostRaster(RendererInfo info, const std::string &source)
    {
        RenderMaterialData material;
        material.backend       = info.backend;
        material.vertexEntry   = "VSMain";
        material.fragmentEntry = "FSMain";
        ShaderCompilerSlang compiler;
        ShaderFileSystem    files;
        files.AddSearchPath(AURORA_SHARED_SHADER_DIR);
        files.AddSearchPath(AURORA_SHADER_INCLUDE_DIR);
        for (const bool vertex : {true, false}) {
            ShaderCompileDesc desc;
            desc.source     = source;
            desc.fileSystem = &files;
            desc.entry      = vertex ? "VSMain" : "FSMain";
            desc.stage      = vertex ? ShaderStageFlagBit::VS : ShaderStageFlagBit::FS;
            desc.target     = info.backend == API::DX12 ? ShaderTarget::DXIL : ShaderTarget::SPIRV;
            ShaderCompileResult compiled;
            EXPECT_TRUE(compiler.Compile(desc, compiled)) << compiled.errorInfo;
            auto &code = vertex ? material.vertexCode : material.fragmentCode;
            code.resize(compiled.data.size() * sizeof(uint32_t));
            std::memcpy(code.data(), compiled.data.data(), code.size());
            for (const auto &resource : compiled.reflection.resources)
                if (std::none_of(material.reflection.resources.begin(), material.reflection.resources.end(),
                                 [&](const auto &item) { return item.set == resource.set && item.binding == resource.binding; }))
                    material.reflection.resources.push_back(resource);
            for (const auto &block : compiled.reflection.blocks)
                if (std::none_of(material.reflection.blocks.begin(), material.reflection.blocks.end(),
                                 [&](const auto &item) { return item.set == block.set && item.binding == block.binding; }))
                    material.reflection.blocks.push_back(block);
        }
        return material;
    }
    inline RenderResourcePayload HostMaterial(RendererInfo info, bool withView = true, bool dynamicPass = false, bool dynamicBatch = true)
    {
        RenderResourcePayload          payload{RenderResourceKey{Uuid::CreateWithSeed(1202), 1, 1, info.deviceEpoch},
                                      Name("runtime-material"),
                                      std::vector<uint8_t>(sizeof(shared::SceneBatchParams)),
                                               {}};
        const shared::SceneBatchParams batch{{1, 0, 0, 1}, RenderIdentityMatrix()};
        std::memcpy(payload.bytes.data(), &batch, sizeof(batch));
        const std::string source = std::string("#include \"SceneBatch.h\"\n") + (withView ? "#include \"ScenePass.h\"\n" : "") +
                                   "float4 VSMain(float2 position : POSITION) : SV_Position { return " +
                                   (withView ? "mul(gScenePass.viewProjection, mul(gSceneBatch.world, float4(position, 0, 1)))"
                                             : "mul(gSceneBatch.world, float4(position, 0, 1))") +
                                   "; }\nfloat4 FSMain() : SV_Target { return gSceneBatch.tint; }";
        auto &material = payload.material.emplace(CompileHostRaster(info, source));
        for (auto &resource : material.reflection.resources) {
            const bool dynamic = resource.set == shared::GetScenePassParamsLayout().set ? dynamicPass : dynamicBatch;
            if (dynamic && resource.type == ShaderResourceType::UNIFORM_BUFFER)
                resource.type = ShaderResourceType::UNIFORM_BUFFER_DYNAMIC;
        }
        return payload;
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

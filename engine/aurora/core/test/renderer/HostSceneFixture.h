//
// Created on 2026/10/17.
//
#pragma once
#include <aurora/renderer/RenderCommands.h>
#include <aurora/shader/ShaderCompilerSlang.h>
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
    inline RenderResourcePayload HostMaterial(RendererInfo info)
    {
        RenderResourcePayload payload{
            RenderResourceKey{Uuid::CreateWithSeed(1202), 1, 1, info.deviceEpoch}, Name("runtime-material"), std::vector<uint8_t>(16), {}};
        const float tint[] = {1, 0, 0, 1};
        std::memcpy(payload.bytes.data(), tint, sizeof(tint));
        auto &material         = payload.material.emplace();
        material.backend       = info.backend;
        material.vertexEntry   = "VSMain";
        material.fragmentEntry = "FSMain";
        ShaderCompilerSlang compiler;
        const std::string   source = R"(
                cbuffer View : register(b0, space0) { column_major float4x4 viewProjection; float4 camera; }
                cbuffer Material : register(b0, space1) { float4 tint; }
                cbuffer Object : register(b0, space2) { column_major float4x4 world; }
                float4 VSMain(float2 position : POSITION) : SV_Position { return mul(viewProjection, mul(world, float4(position, 0, 1))); }
                float4 FSMain() : SV_Target { return tint; }
            )";
        for (const bool vertex : {true, false}) {
            ShaderCompileDesc desc;
            desc.source = source;
            desc.entry  = vertex ? "VSMain" : "FSMain";
            desc.stage  = vertex ? ShaderStageFlagBit::VS : ShaderStageFlagBit::FS;
            desc.target = info.backend == API::DX12 ? ShaderTarget::DXIL : ShaderTarget::SPIRV;
            ShaderCompileResult compiled;
            EXPECT_TRUE(compiler.Compile(desc, compiled)) << compiled.errorInfo;
            auto &code = vertex ? material.vertexCode : material.fragmentCode;
            code.resize(compiled.data.size() * sizeof(uint32_t));
            std::memcpy(code.data(), compiled.data.data(), code.size());
        }
        material.reflection.resources = {{"View", ShaderResourceType::UNIFORM_BUFFER_DYNAMIC, 0, 0, 1},
                                         {"Material", ShaderResourceType::UNIFORM_BUFFER, 1, 0, 1},
                                         {"Object", ShaderResourceType::UNIFORM_BUFFER_DYNAMIC, 2, 0, 1}};
        material.reflection.blocks    = {{"View", "", 0, 0, 80}, {"Material", "", 1, 0, 16}, {"Object", "", 2, 0, 64}};
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

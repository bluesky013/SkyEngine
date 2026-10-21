//
// Created on 2026/10/17.
//

#pragma once

#include <aurora/rdg/FrozenFrameGraph.h>
#include <aurora/rhi/ShaderReflection.h>
#include <core/name/Name.h>

namespace sky::aurora {
    struct RenderMeshStream {
        uint64_t        offset = 0, size = 0;
        uint32_t        stride = 0;
        VertexInputRate rate   = VertexInputRate::PER_VERTEX;
    };
    struct RenderMeshLod {
        uint64_t                    offset = 0, size = 0;
        IndexType                   type = IndexType::U32;
        CmdDrawIndexed              command;
        std::vector<CmdDrawIndexed> sections;
    };
    struct RenderMeshData {
        std::vector<RenderMeshStream>      streams;
        std::vector<RasterVertexAttribute> attributes;
        std::vector<RenderMeshLod>         lods;
    };
    // Owned compiled code and layout; native shader objects are created only by the coordinator.
    struct RenderTechniqueData {
        API                  backend = API::DEFAULT;
        std::vector<uint8_t> vertexCode, fragmentCode, computeCode;
        std::string          vertexEntry, fragmentEntry, computeEntry;
        ShaderReflection     reflection;
        ShaderSpecialization specialization;
        BlendState           blend;
        RasterDepthState     depth;
    };
    struct RenderUniformData {
        uint32_t               binding = 0;
        std::vector<std::byte> bytes;
    };
    struct RenderMaterialTechnique {
        Name                           tag;
        RenderTechniqueData            program;
        std::vector<RenderUniformData> uniforms;
    };
    struct RenderMaterialData {
        std::vector<RenderMaterialTechnique> techniques;
    };
} // namespace sky::aurora

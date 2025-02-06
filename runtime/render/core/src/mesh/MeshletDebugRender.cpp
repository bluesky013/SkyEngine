//
// Created by blues on 2025/2/7.
//

#include <render/mesh/MeshletDebugRender.h>
#include <render/resource/Meshlet.h>
#include <render/RenderTechniqueLibrary.h>

namespace sky {

    void MeshletDebugRender::BuildGeometry(const RDBufferPtr &meshlet)
    {
        std::vector<uint16_t> indices;
        indices.reserve(MeshletConePrimitive::CONE_SEGMENT * 3);

        for (uint16_t i = 0; i < MeshletConePrimitive::CONE_SEGMENT; ++i) {
            uint16_t idx1 = i + 1;
            uint16_t idx2 = (i + 1) % MeshletConePrimitive::CONE_SEGMENT + 1;

            indices.emplace_back(0);
            indices.emplace_back(idx1);
            indices.emplace_back(idx2);
        }
        auto indicesCount = static_cast<uint32_t>(indices.size());
        size_t size = static_cast<uint32_t>(indicesCount * sizeof(uint16_t));
        rhi::BufferUploadRequest request = {};
        request.source = new rhi::TRawBufferStream<uint16_t>(std::move(indices));
        request.size = size;

        conePrimitive->geometry = new RenderGeometry();
        conePrimitive->geometry->AddVertexAttribute(VertexAttribute{VertexSemanticFlagBit::CUSTOM0, 0, OFFSET_OF(Meshlet, center), rhi::Format::F_RGBA32, rhi::VertexInputRate::PER_INSTANCE});
        conePrimitive->geometry->AddVertexAttribute(VertexAttribute{VertexSemanticFlagBit::CUSTOM1, 0, OFFSET_OF(Meshlet, coneApex), rhi::Format::F_RGBA32, rhi::VertexInputRate::PER_INSTANCE});
        conePrimitive->geometry->AddVertexAttribute(VertexAttribute{VertexSemanticFlagBit::CUSTOM2, 0, OFFSET_OF(Meshlet, coneAxis), rhi::Format::F_RGBA32, rhi::VertexInputRate::PER_INSTANCE});

        conePrimitive->geometry->indexBuffer.buffer = new Buffer();
        conePrimitive->geometry->indexBuffer.buffer->Init(size, rhi::BufferUsageFlagBit::INDEX | rhi::BufferUsageFlagBit::TRANSFER_DST, rhi::MemoryType::CPU_TO_GPU);
        conePrimitive->geometry->indexBuffer.buffer->SetSourceData(request);
        conePrimitive->geometry->indexBuffer.offset = 0;
        conePrimitive->geometry->indexBuffer.range = size;
        conePrimitive->geometry->indexBuffer.indexType = rhi::IndexType::U16;
        conePrimitive->geometry->Upload();

        conePrimitive->geometry->vertexBuffers.emplace_back(VertexBuffer{meshlet, 0, meshlet->GetSize(), sizeof(Meshlet)});
        conePrimitive->geometry->version++;

        auto count = static_cast<uint32_t>(meshlet->GetSize() / sizeof(Meshlet));
        conePrimitive->args.emplace_back(rhi::CmdDrawIndexed {
            indicesCount, count, 0, 0, 0
        });
    }

    void MeshletDebugRender::BuildBatch()
    {
        auto tech = RenderTechniqueLibrary::Get()->FetchGfxTechnique(Name("techniques/meshlet_debug.tech"));
        if (tech) {
            conePrimitive->batches.emplace_back(RenderBatch {
                tech
            });
        }
    }

    void MeshletDebugRender::Setup(const RDBufferPtr &meshlet)
    {
        conePrimitive = std::make_unique<MeshletConePrimitive>();
        BuildGeometry(meshlet);
        BuildBatch();
    }

} // namespace sky
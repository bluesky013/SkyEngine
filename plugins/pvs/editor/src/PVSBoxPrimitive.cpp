//
// Created by Zach Lee on 2026/3/1.
//

#include <pvs/editor/PVSBoxPrimitive.h>
#include <render/Renderer.h>

namespace sky::editor {

    PVSBoxGeometry::PVSBoxGeometry()
    {
        std::vector<Vector3> vertices = {
            {-0.5f, -0.5f, -0.5f},
            { 0.5f, -0.5f, -0.5f},
            { 0.5f, -0.5f,  0.5f},
            {-0.5f, -0.5f,  0.5f},

            {-0.5f,  0.5f, -0.5f},
            { 0.5f,  0.5f, -0.5f},
            { 0.5f,  0.5f,  0.5f},
            {-0.5f,  0.5f,  0.5f},
        };

        std::vector<uint16_t> indices = {
            // bottom (y = -0.5), normal -Y
            0, 1, 2,  0, 2, 3,
            // top (y = +0.5), normal +Y
            4, 6, 5,  4, 7, 6,
            // front (z = +0.5), normal +Z
            3, 2, 6,  3, 6, 7,
            // back (z = -0.5), normal -Z
            0, 5, 1,  0, 4, 5,
            // right (x = +0.5), normal +X
            1, 5, 6,  1, 6, 2,
            // left (x = -0.5), normal -X
            0, 3, 7,  0, 7, 4,
        };

        size_t vertexBufferSize = vertices.size() * sizeof(Vector3);
        size_t indexBufferSize = indices.size() * sizeof(uint16_t);

        auto positionBuffer = new Buffer();
        positionBuffer->Init(vertexBufferSize, rhi::BufferUsageFlagBit::VERTEX, rhi::MemoryType::CPU_TO_GPU);
        {
            auto *ptr = positionBuffer->GetRHIBuffer()->Map();
            memcpy(ptr, vertices.data(), vertexBufferSize);
        }

        indexBuffer.indexType = rhi::IndexType::U16;
        indexBuffer.buffer = new Buffer();
        indexBuffer.buffer->Init(indexBufferSize, rhi::BufferUsageFlagBit::INDEX, rhi::MemoryType::CPU_TO_GPU);
        {
            auto *ptr = indexBuffer.buffer->GetRHIBuffer()->Map();
            memcpy(ptr, indices.data(), indexBufferSize);
        }

        vertexBuffers.resize(2);
        vertexBuffers[0] = VertexBuffer{positionBuffer, 0, static_cast<uint32_t>(vertexBufferSize), sizeof(Vector3), rhi::VertexInputRate::PER_VERTEX};

        vertexAttributes = {
            VertexAttribute{VertexSemanticFlagBit::POSITION, 0, 0, rhi::Format::F_RGB32},
            VertexAttribute{VertexSemanticFlagBit::INST0, 1, OFFSET_OF(PVSBoxInstanceBuffer, center), rhi::Format::F_RGB32},
            VertexAttribute{VertexSemanticFlagBit::INST1, 1, OFFSET_OF(PVSBoxInstanceBuffer, scale), rhi::Format::F_RGB32},
            VertexAttribute{VertexSemanticFlagBit::INST2, 1, OFFSET_OF(PVSBoxInstanceBuffer, id), rhi::Format::U_R32},
        };

        attributeSemantics = VertexSemanticFlagBit::POSITION |
            VertexSemanticFlagBit::INST0 |
            VertexSemanticFlagBit::INST1 |
            VertexSemanticFlagBit::INST2;
    }

    void PVSGeometryPrimitive::Update(const std::vector<PVSDrawGeometryInstance>& inInstances)
    {
        uint32_t instanceCount = static_cast<uint32_t>(inInstances.size());

        meshes.resize(instanceCount);
        buffers.resize(instanceCount);
        resourceGroups.resize(instanceCount);
        pipelines.resize(instanceCount);

        for (uint32_t i = 0; i < instanceCount; ++i) {
            meshes[i] = inInstances[i].mesh;

            buffers[i] = new Buffer();
            buffers[i]->Init(sizeof(PVSBatchBufferData), rhi::BufferUsageFlagBit::UNIFORM, rhi::MemoryType::CPU_TO_GPU);
            auto *ptr = buffers[i]->GetRHIBuffer()->Map();
            PVSBatchBufferData instanceData = {
                .world = inInstances[i].transform,
                .id = inInstances[i].id,
            };
            memcpy(ptr, &instanceData, sizeof(PVSBatchBufferData));
        }
    }

    void PVSGeometryPrimitive::Reset()
    {
        meshes.clear();
        buffers.clear();
        resourceGroups.clear();
        pipelines.clear();
    }

    bool PVSGeometryPrimitive::PrepareBatch(const RenderBatchPrepareInfo& info) noexcept
    {
        if (info.techId != techInst.technique->GetRasterID()) {
            return false;
        }

        if (techInst.UpdateProgram(info.pipelineKey)) {
            auto pipelineState = techInst.technique->GetPipelineState();;
            layout = techInst.program->RequestLayout(BATCH_SET);

            for (uint32_t i = 0; i < resourceGroups.size(); ++i) {
                resourceGroups[i] = new ResourceGroup();
                resourceGroups[i]->Init(layout, *Renderer::Get()->GetMaterialManager()->GetPool());
                resourceGroups[i]->BindBuffer(Name("Batch"), buffers[i]->GetRHIBuffer(), 0);
                resourceGroups[i]->Update();

                const auto& geometry = meshes[i]->GetGeometry();
                geometry->Upload();
                pipelines[i] = GraphicsTechnique::BuildPso(techInst.program,
                    pipelineState,
                    geometry->Request(techInst.program),
                    info.pass,
                    info.subPassId);
            }

            Renderer::Get()->GetStreamingManager()->FlushAll();
        }

        return true;
    }

    void PVSGeometryPrimitive::GatherRenderItem(IRenderItemGatherContext* context) noexcept
    {
        for (uint32_t i = 0; i < meshes.size(); ++i) {
            const auto &mesh = meshes[i];
            const auto &subMeshes = mesh->GetSubMeshes();
            const auto &batchGroup = resourceGroups[i];

            for (const auto &sub : subMeshes) {
                context->Append(RenderItem {
                    .geometry = mesh->GetGeometry(),
                    .batchGroup = batchGroup,
                    .pso = pipelines[i],
                    .args = {rhi::CmdDrawIndexed {
                        sub.indexCount,
                        1,
                        sub.firstIndex,
                        static_cast<int32_t>(sub.firstVertex),
                        0
                    }}
                });
            }
        }
    }

} // namespace sky::editor
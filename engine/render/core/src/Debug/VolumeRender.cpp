//
// Created by Zach Lee on 2026/2/24.
//

#include <render/debug/VolumeRenderer.h>
#include <render/RenderScene.h>

namespace sky {

    VolumeBoxLineGeometry::VolumeBoxLineGeometry()
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
            0, 1, 1, 2, 2, 3, 3, 0,
            4, 5, 5, 6, 6, 7, 7, 4,
            0, 4, 1, 5, 2, 6, 3, 7
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
            VertexAttribute{VertexSemanticFlagBit::INST0, 1, OFFSET_OF(BoxGeometryInstanceData, center), rhi::Format::F_RGB32},
            VertexAttribute{VertexSemanticFlagBit::INST1, 1, OFFSET_OF(BoxGeometryInstanceData, extent), rhi::Format::F_RGB32},
            VertexAttribute{VertexSemanticFlagBit::INST2, 1, OFFSET_OF(BoxGeometryInstanceData, color), rhi::Format::F_RGBA32},
        };

        attributeSemantics = VertexSemanticFlagBit::POSITION |
            VertexSemanticFlagBit::INST0 |
            VertexSemanticFlagBit::INST1 |
            VertexSemanticFlagBit::INST2;

    }

    VolumeRenderer::VolumeRenderer(RenderScene* inScene, const RDTechniquePtr &tech)
        : scene(inScene)
    {
        primitive = std::make_unique<VolumePrimitive>(tech);
        scene->AddPrimitive(primitive.get());
    }

    VolumeRenderer::~VolumeRenderer() noexcept
    {
        ResetPrimitive();
    }

    void VolumeRenderer::ResetPrimitive() noexcept
    {
        if (primitive) {
            scene->RemovePrimitive(primitive.get());
            primitive.reset();
        }
    }

    void VolumeRenderer::Draw(const AABB& bounding, const Color& color) noexcept
    {
        Vector3 extent = (bounding.max - bounding.min) * 0.5f;
        Vector3 center = bounding.min + extent;

        data.emplace_back(BoxGeometryInstanceData {
            center,
            extent / 0.5f,
            color
        });
    }

    void VolumeRenderer::Draw(const BoundingBox& bounding, const Color& color) noexcept
    {
        data.emplace_back(BoxGeometryInstanceData {
            bounding.center,
            bounding.extent / 0.5f,
            color
        });
    }

    void VolumeRenderer::Draw(const BoundingBoxSphere& bounding, const Color& color) noexcept
    {
        data.emplace_back(BoxGeometryInstanceData {
            bounding.center,
            bounding.extent / 0.5f,
            color
        });
    }

    void VolumeRenderer::Update() noexcept
    {
        if (primitive) {
            std::vector<BoxGeometryInstanceData> tmpData;
            tmpData.swap(data);

            primitive->Update(std::move(tmpData));
        }
    }

    void VolumePrimitive::Update(std::vector<BoxGeometryInstanceData>&& data) noexcept
    {
        args = rhi::CmdDrawIndexed{
            .indexCount = 24,
            .instanceCount = static_cast<uint32_t>(data.size()),
            .firstIndex = 0,
            .vertexOffset = 0,
            .firstInstance = 0,
        };

        auto dataSize = static_cast<uint32_t>(data.size() * sizeof(BoxGeometryInstanceData));
        if (!instanceBuffer || dataSize > instanceBuffer->GetSize()) {
            instanceBuffer = new DynamicBuffer();
            instanceBuffer->Init(dataSize, rhi::BufferUsageFlagBit::VERTEX);

            geometry->vertexBuffers[1] = VertexBuffer{
                instanceBuffer, 0, dataSize, sizeof(BoxGeometryInstanceData), rhi::VertexInputRate::PER_INSTANCE
            };
        }
        instanceBuffer->SwapBuffer();
        instanceBuffer->Update(reinterpret_cast<const uint8_t *>(data.data()), 0, dataSize);
    }

    bool VolumePrimitive::PrepareBatch(const RenderBatchPrepareInfo& info) noexcept
    {
        if (info.techId != techInst.technique->GetRasterID()) {
            return false;
        }

        if (techInst.UpdateProgram(info.pipelineKey)) {
            auto pipelineState = techInst.technique->GetPipelineState();;
            pipelineState.inputAssembly.topology = rhi::PrimitiveTopology::LINE_LIST;

            pso = GraphicsTechnique::BuildPso(techInst.program,
                pipelineState,
                geometry->Request(techInst.program),
                info.pass,
                info.subPassId);
        }

        return !!pso;
    }

    void VolumePrimitive::GatherRenderItem(IRenderItemGatherContext* context) noexcept
    {
        context->Append(RenderItem {
            .geometry = geometry,
            .batchGroup = nullptr,
            .pso = pso,
            .args = {args}
        });
    }

} // namespace sky
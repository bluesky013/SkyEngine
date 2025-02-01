//
// Created by blues on 2024/11/7.
//

#include <terrain/TerrainRender.h>
#include <terrain/TerrainUtils.h>
#include <core/platform/Platform.h>
#include <core/math/MathUtil.h>
#include <render/RenderScene.h>
#include <render/RHI.h>

namespace sky {
    TerrainRender::TerrainRender(const TerrainQuad& quad) : config(quad)
    {
        BuildGeometry();
    }

    void TerrainRender::Tick()
    {

    }

    void TerrainRender::AddSector(const TerrainSector &sector)
    {
        sectors.emplace_back(sector);
    }

    void TerrainRender::RemoveSector(const TerrainCoord &coord)
    {
        sectors.erase(std::remove_if(sectors.begin(), sectors.end(),
            [&coord](const auto &v) { return v.coord.x == coord.x && v.coord.y == coord.y;
            }), sectors.end());
    }

    bool TerrainRender::HasSector(const TerrainCoord &coord) const
    {
        return std::find_if(sectors.begin(), sectors.end(),
            [&coord](const auto &v) { return v.coord.x == coord.x && v.coord.y == coord.y;
            }) != sectors.end();
    }

    void TerrainRender::BuildGeometry()
    {
        auto size = ConvertSectionSize(config.sectionSize);
        uint8_t mips = CeilLog2(size + 1);

        BuildVertexBuffer(config, vertexBuffer);
        indexBuffers.resize(mips);
        for (int32_t mip = static_cast<int>(mips) - 1; mip >= 0; mip--) {
            BuildIndexBufferLod(config, indexBuffers[mip], mip);
        }
    }

    void TerrainRender::SetMaterial(const RDMaterialInstancePtr &mat)
    {
        material = mat;
    }

    void TerrainRender::DetachFromScene(RenderScene* scene)
    {

    }

    void TerrainRender::AttachToScene(RenderScene* scene)
    {
        for (auto &sec : sectorRenders) {
            scene->AddPrimitive(sec->GetPrimitive());
        }
    }

    void TerrainRender::BuildSectors()
    {
        auto sectorNum = sectors.size();
        sectorRenders.resize(sectors.size());

        auto sectorSize = ConvertSectionSize(config.sectionSize);

        auto *queue = RHI::Get()->GetDevice()->GetQueue(rhi::QueueType::TRANSFER);

        std::vector<TerrainInstanceData> instanceDatas(sectorNum, TerrainInstanceData{});
        std::vector<uint32_t> instances(sectorNum);
        for (uint32_t i = 0; i < sectors.size(); ++i) {
            instanceDatas[i].offsetX = static_cast<float>(sectors[i].coord.x * sectorSize);
            instanceDatas[i].offsetY = static_cast<float>(sectors[i].coord.y * sectorSize);
            instanceDatas[i].sectorWidth  = sectorSize;
            instanceDatas[i].sectorHeight = sectorSize;
            instanceDatas[i].resolution = config.resolution;

            instances[i] = i;
            sectors[i].heightMap->IStreamableResource::Upload(queue);
        }
        // build instance buffer
        instanceBuffer.buffer = new Buffer();
        instanceBuffer.buffer->Init(sectorNum * sizeof(uint32_t), rhi::BufferUsageFlagBit::VERTEX | rhi::BufferUsageFlagBit::TRANSFER_DST, rhi::MemoryType::GPU_ONLY);
        instanceBuffer.buffer->SetSourceData(std::move(instances));
        instanceBuffer.buffer->Upload(queue);
        instanceBuffer.buffer->Wait();
        instanceBuffer.range = sectorNum * sizeof(uint32_t);
        instanceBuffer.stride = sizeof(uint32_t);

        // build instance data buffer
        instanceDataBuffer = new Buffer();
        instanceDataBuffer->Init(sectorNum * sizeof(TerrainInstanceData), rhi::BufferUsageFlagBit::STORAGE | rhi::BufferUsageFlagBit::TRANSFER_DST, rhi::MemoryType::GPU_ONLY);
        instanceDataBuffer->SetSourceData(std::move(instanceDatas));
        instanceDataBuffer->Upload(queue);
        instanceDataBuffer->Wait();

        CounterPtr<RenderGeometry> geometry = new RenderGeometry();
        // build geometry
        geometry->vertexBuffers.emplace_back(vertexBuffer);
        geometry->vertexBuffers.emplace_back(instanceBuffer);
        geometry->indexBuffer = indexBuffers[0];
        geometry->attributeSemantics = VertexSemanticFlagBit::POSITION | VertexSemanticFlagBit::INST0;
        geometry->vertexAttributes.emplace_back(VertexAttribute{VertexSemanticFlagBit::POSITION, 0, 0, rhi::Format::U_RG8});
        geometry->vertexAttributes.emplace_back(VertexAttribute{VertexAttribute{VertexSemanticFlagBit::INST0, 1, 0, rhi::Format::U_R32, rhi::VertexInputRate::PER_INSTANCE}});
        geometry->version++;

//        batchSet = material->GetResourceGroup();
        batchSet->BindBuffer(Name("InstancedData"), instanceDataBuffer->GetRHIBuffer(), 0);
        batchSet->Update();

        for (uint32_t i = 0; i < sectors.size(); ++i) {
            sectorRenders[i] = std::make_unique<TerrainSectorRender>(sectors[i].coord);
            sectorRenders[i]->SetGeometry(geometry, i);
            sectorRenders[i]->SetMaterial(material, batchSet);
            sectorRenders[i]->SetHeightMap(sectors[i].heightMap);
        }
    }

	void TerrainRender::BuildVertexBuffer(const TerrainQuad &quad, VertexBuffer &vertexBuffer)
	{
		auto size = ConvertSectionSize(quad.sectionSize) + 1;

		vertexBuffer.offset = 0;
		vertexBuffer.range  = static_cast<uint32_t>(sizeof(TerrainVertex)) * size * size;
		vertexBuffer.stride = static_cast<uint32_t>(sizeof(TerrainVertex));
		vertexBuffer.buffer = new Buffer();
		vertexBuffer.buffer->Init(vertexBuffer.range, rhi::BufferUsageFlagBit::VERTEX, rhi::MemoryType::CPU_TO_GPU);

		auto *ptr = reinterpret_cast<TerrainVertex*>(vertexBuffer.buffer->GetRHIBuffer()->Map());

        for (uint32_t y = 0; y < size; ++y) {
            for (uint32_t x = 0; x < size; ++x) {
                ptr->x = static_cast<uint8_t>(x);
                ptr->y = static_cast<uint8_t>(y);
                ++ptr;
            }
        }

        vertexBuffer.buffer->GetRHIBuffer()->UnMap();
	}

	void TerrainRender::BuildIndexBufferLod(const TerrainQuad &quad, IndexBuffer &indexBuffer, uint32_t lod)
	{
        auto size = ConvertSectionSize(quad.sectionSize) + 1;

        std::vector<uint16_t> indices;
		uint32_t subsectionSizeQuads = (size >> lod) - 1;

        indexBuffer.indexType = rhi::IndexType::U16;

        indexBuffer.offset = 0;
		indexBuffer.range  = static_cast<uint32_t>(sizeof(uint16_t)) * subsectionSizeQuads * subsectionSizeQuads * 6;
		indexBuffer.buffer = new Buffer();
		indexBuffer.buffer->Init(indexBuffer.range, rhi::BufferUsageFlagBit::INDEX, rhi::MemoryType::CPU_TO_GPU);

        for (uint32_t y = 0; y < subsectionSizeQuads; ++y) {
            for (uint32_t x = 0; x < subsectionSizeQuads; ++x) {

                auto i00 = static_cast<uint16_t>((x + 0) + (y + 0) * size);
                auto i10 = static_cast<uint16_t>((x + 1) + (y + 0) * size);
                auto i11 = static_cast<uint16_t>((x + 1) + (y + 1) * size);
                auto i01 = static_cast<uint16_t>((x + 0) + (y + 1) * size);

                indices.emplace_back(i00);
                indices.emplace_back(i10);
                indices.emplace_back(i11);

                indices.emplace_back(i11);
                indices.emplace_back(i01);
                indices.emplace_back(i00);
            }
        }

        auto *ptr = indexBuffer.buffer->GetRHIBuffer()->Map();
        SKY_ASSERT(indexBuffer.range == indices.size() * sizeof(uint16_t));
        memcpy(ptr, indices.data(), indices.size() * sizeof(uint16_t));
        indexBuffer.buffer->GetRHIBuffer()->UnMap();
	}

} // namespace sky
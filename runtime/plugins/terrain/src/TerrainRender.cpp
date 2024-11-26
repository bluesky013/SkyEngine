//
// Created by blues on 2024/11/7.
//

#include <terrain/TerrainRender.h>
#include <terrain/TerrainUtils.h>
#include <core/platform/Platform.h>
#include <core/math/MathUtil.h>

namespace sky {
    TerrainRender::TerrainRender(const TerrainQuad& quad) : config(quad)
    {
        BuildGeometry();
    }

    void TerrainRender::Tick()
    {

    }

    void TerrainRender::BuildGeometry()
    {
        auto size = ConvertSectionSize(config.sectionSize);
        uint8_t mips = CeilLog2(size);

        BuildVertexBuffer(config, vertexBuffer);
        indexBuffers.resize(mips);
        for (int32_t mip = static_cast<int>(mips) - 1; mip >= 0; mip--) {
            BuildIndexBufferLod(config, indexBuffers[mip], mip);
        }
    }

	void TerrainRender::BuildVertexBuffer(const TerrainQuad &quad, VertexBuffer &vertexBuffer)
	{
		auto size = ConvertSectionSize(quad.sectionSize);

		vertexBuffer.offset = 0;
		vertexBuffer.range  = static_cast<uint32_t>(sizeof(TerrainVertex)) * size * size * quad.subSection * quad.subSection;
		vertexBuffer.stride = static_cast<uint32_t>(sizeof(TerrainVertex));
		vertexBuffer.buffer = new Buffer();
		vertexBuffer.buffer->Init(vertexBuffer.range, rhi::BufferUsageFlagBit::VERTEX, rhi::MemoryType::CPU_TO_GPU);

		auto *ptr = reinterpret_cast<TerrainVertex*>(vertexBuffer.buffer->GetRHIBuffer()->Map());

		for (uint32_t x = 0; x < quad.subSection; ++x) {
            for (uint32_t y = 0; y < quad.subSection; ++y) {
                for (uint32_t i = 0; i < size; ++i) {
                    for (uint32_t j = 0; j < size; ++j) {
                        ptr->x = static_cast<uint8_t>(i);
                        ptr->y = static_cast<uint8_t>(j);
                        ptr->subx = x;
                        ptr->suby = y;
                        ++ptr;
                    }
                }
            }
		}

        vertexBuffer.buffer->GetRHIBuffer()->UnMap();
	}

	void TerrainRender::BuildIndexBufferLod(const TerrainQuad &quad, IndexBuffer &indexBuffer, uint32_t lod)
	{
        auto size = ConvertSectionSize(quad.sectionSize);

        std::vector<uint16_t> indices;
		uint32_t subsectionSizeQuads = (size >> lod) - 1;

        indexBuffer.indexType = rhi::IndexType::U16;

        indexBuffer.offset = 0;
		indexBuffer.range  = static_cast<uint32_t>(sizeof(uint16_t)) * subsectionSizeQuads * subsectionSizeQuads * quad.subSection * quad.subSection * 6;
		indexBuffer.buffer = new Buffer();
		indexBuffer.buffer->Init(indexBuffer.range, rhi::BufferUsageFlagBit::INDEX, rhi::MemoryType::CPU_TO_GPU);

        uint32_t SubOffset = 0;        
		for (uint32_t x = 0; x < quad.subSection; ++x) {
            for (uint32_t y = 0; y < quad.subSection; ++y) {

                for (uint32_t i = 0; i < subsectionSizeQuads; ++i) {
                    for (uint32_t j = 0; j < subsectionSizeQuads; ++j) {

                        auto i00 = static_cast<uint16_t>((x + 0) + (y + 0) * size + SubOffset);
                        auto i10 = static_cast<uint16_t>((x + 1) + (y + 0) * size + SubOffset);
                        auto i11 = static_cast<uint16_t>((x + 1) + (y + 1) * size + SubOffset);
                        auto i01 = static_cast<uint16_t>((x + 0) + (y + 1) * size + SubOffset);

                        indices.emplace_back(i00);
                        indices.emplace_back(i10);
                        indices.emplace_back(i11);

                        
                        indices.emplace_back(i11);
                        indices.emplace_back(i01);
                        indices.emplace_back(i00);
                    }
                }
            }
        }

        auto *ptr = indexBuffer.buffer->GetRHIBuffer()->Map();
        SKY_ASSERT(indexBuffer.range == indices.size() * sizeof(uint16_t));
        memcpy(ptr, indices.data(), indices.size() * sizeof(uint16_t));
        indexBuffer.buffer->GetRHIBuffer()->UnMap();
	}

} // namespace sky
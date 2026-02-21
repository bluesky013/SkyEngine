//
// Created by blues on 2023/12/18.
//

#include <render/env/SkySphereRenderer.h>
#include <render/Renderer.h>
#include <render/RHI.h>

namespace sky {

    static const rhi::DescriptorSetPool::PoolSize POOL_DESC[] = {
            rhi::DescriptorSetPool::PoolSize{rhi::DescriptorType::SAMPLED_IMAGE, 1},
            rhi::DescriptorSetPool::PoolSize{rhi::DescriptorType::SAMPLER, 1},
    };

    // void SkySpherePrimitive::UpdateBatch()
    // {
        // auto &batch = sections[0].batches[0];
        // if (!batch.batchGroup && batch.program) {
        //     auto layout = batch.program->RequestLayout(BATCH_SET);
        //     batch.batchGroup = new ResourceGroup();
        //     batch.batchGroup->Init(layout, *pool);
        //     batch.batchGroup->BindTexture(Name("SkyBox"), texture->GetImageView(), 0);
        //     batch.batchGroup->Update();
        // }
    // }

    bool SkySpherePrimitive::IsReady() const noexcept
    {
        return texture && texture->IsReady();
    }

    SkySphereRenderer::SkySphereRenderer()
    {
        pool = RHI::Get()->GetDevice()->CreateDescriptorSetPool({
            1, sizeof(POOL_DESC) / sizeof(rhi::DescriptorSetPool::PoolSize), POOL_DESC
        });

        primitive = std::make_unique<SkySpherePrimitive>();
        primitive->geometry = new RenderGeometry();
        primitive->geometry->AddVertexAttribute(VertexAttribute{VertexSemanticFlagBit::POSITION, 0, OFFSET_OF(SkyBoxVertex, pos), rhi::Format::F_RGBA32});
        primitive->geometry->AddVertexAttribute(VertexAttribute{VertexSemanticFlagBit::UV,       0, OFFSET_OF(SkyBoxVertex, uv),  rhi::Format::F_RG32});

        BuildSphere();
    }

    void SkySphereRenderer::BuildSphere()
    {
        static const uint32_t STACK_COUNT = 16;
        static const uint32_t SECTOR_COUNT = 32;

        float stackStep   = PI / STACK_COUNT;
        float sectorStep  = 2 * PI / SECTOR_COUNT;

        std::vector<SkyBoxVertex> vertices;
        std::vector<uint32_t> indices;
        for(int i = 0; i <= STACK_COUNT; ++i)                // starting from pi/2 to -pi/2
        {
            float stackAngle1 = PI / 2 - static_cast<float>(i) * stackStep;

            float xz = radius * cosf(stackAngle1);
            float y  = radius * sinf(stackAngle1);

            for(int j = 0; j <= SECTOR_COUNT; ++j)           // starting from 0 to 2pi
            {
                float sectorAngle = static_cast<float>(j) * sectorStep;

                SkyBoxVertex vertex = {};
                vertex.pos.x = xz * cosf(sectorAngle);
                vertex.pos.z = xz * sinf(sectorAngle);
                vertex.pos.y = y;

                vertex.uv.x = static_cast<float>(j) / static_cast<float>(SECTOR_COUNT);
                vertex.uv.y = static_cast<float>(i) / static_cast<float>(STACK_COUNT);

                vertices.emplace_back(vertex);
            }
        }

        uint32_t k1;
        uint32_t k2;
        for(int i = 0; i < STACK_COUNT; ++i)
        {
            k1 = i * (SECTOR_COUNT + 1);
            k2 = k1 + SECTOR_COUNT + 1;

            for(int j = 0; j < SECTOR_COUNT; ++j, ++k1, ++k2)
            {
                if (i != 0)
                {
                    indices.push_back(k1);
                    indices.push_back(k2);
                    indices.push_back(k1 + 1);
                }

                if (i != (STACK_COUNT - 1))
                {
                    indices.push_back(k1 + 1);
                    indices.push_back(k2);
                    indices.push_back(k2 + 1);
                }
            }
        }
        rhi::CmdDrawIndexed indexed = {};
        indexed.indexCount = static_cast<uint32_t>(indices.size());
        // primitive->sections[0].args.emplace_back(indexed);

        primitive->geometry->vertexBuffers.clear();

        auto* vb = new Buffer();
        vb->Init(vertices.size() * sizeof(SkyBoxVertex), rhi::BufferUsageFlagBit::VERTEX | rhi::BufferUsageFlagBit::TRANSFER_DST, rhi::MemoryType::GPU_ONLY);
        vb->SetUploadData<SkyBoxVertex>(std::move(vertices));
        primitive->geometry->vertexBuffers.emplace_back(VertexBuffer{
            vb, 0, vb->GetSize(), sizeof(SkyBoxVertex)
        });

        auto* ib = new Buffer();
        ib->Init(indices.size() * sizeof(uint32_t), rhi::BufferUsageFlagBit::INDEX | rhi::BufferUsageFlagBit::TRANSFER_DST, rhi::MemoryType::GPU_ONLY);
        ib->SetUploadData(std::move(indices));
        primitive->geometry->indexBuffer.buffer = ib;
        primitive->geometry->indexBuffer.range = ib->GetSize();
        primitive->geometry->indexBuffer.indexType = rhi::IndexType::U32;

        Renderer::Get()->GetStreamingManager()->UploadBuffer(vb);
        Renderer::Get()->GetStreamingManager()->UploadBuffer(ib);
        primitive->pool = pool;

//        resourceGroup = new ResourceGroup();
//        resourceGroup->Init(technique->RequestProgram({})->RequestLayout(BATCH_SET), *pool);
//        primitive->batches[0].batchGroup = resourceGroup;
    }

    SkySphereRenderer::~SkySphereRenderer() = default;

    void SkySphereRenderer::SetTechnique(const RDGfxTechPtr &tech)
    {
        // technique = tech;
        // primitive->sections.emplace_back();
        // primitive->sections[0].batches.emplace_back(RenderBatch{
        //     technique
        // });
    }

} // namespace sky
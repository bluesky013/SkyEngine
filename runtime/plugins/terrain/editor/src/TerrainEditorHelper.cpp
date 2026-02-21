//
// Created by blues on 2024/11/30.
//

#include <terrain/editor/TerrainToolHelper.h>
#include <terrain/TerrainUtils.h>
#include <framework/asset/AssetManager.h>
#include <render/adaptor/assets/TechniqueAsset.h>

namespace sky::editor {

    struct TerrainPrimitive : public RenderPrimitive {

    };

    TerrainHelper::TerrainHelper()
        : renderer(std::make_unique<DebugRenderer>())
    {
        auto techAsset = AssetManager::Get()->LoadAssetFromPath<Technique>("techniques/debug.tech");
        techAsset->BlockUntilLoaded();
        technique = CreateTechniqueFromAsset(techAsset);

        // primitives.resize(5);
        // for (auto &prim : primitives) {
        //     RenderBatch batch = {technique};
        //     batch.topo = rhi::PrimitiveTopology::TRIANGLE_LIST;
        //
        //     prim = new TerrainPrimitive();
        //     prim->batches.emplace_back(batch);
        // }
    }

    TerrainHelper::~TerrainHelper()
    {
        for (auto &prim : primitives) {
            delete prim;
        }
    }

    void TerrainHelper::Reset()
    {
        renderer->Reset();
    }

    void TerrainHelper::DrawFullTerrainGrid(const TerrainBuildConfig &cfg, const Vector3 &worldPos)
    {
        renderer->Reset();

        renderer->SetTopo(rhi::PrimitiveTopology::TRIANGLE_LIST);
        renderer->SetBlendEnable(true);
        renderer->SetColor(Color32{0, 32, 0, 128});

        auto size = ConvertSectionSize(cfg.sectionSize);

        float boundX = static_cast<float>(cfg.sectionNumX) * static_cast<float>(size);
        float boundY = static_cast<float>(cfg.sectionNumY) * static_cast<float>(size);
        // Draw Bounds
        {
            float x0 = 0;
            float y0 = 0;

            float x1 = boundX * cfg.resolution;
            float y1 = boundY * cfg.resolution;

            Vector3 p00 = worldPos + Vector3(x0, 0.f, y0);
            Vector3 p10 = worldPos + Vector3(x1, 0.f, y0);
            Vector3 p11 = worldPos + Vector3(x1, 0.f, y1);
            Vector3 p01 = worldPos + Vector3(x0, 0.f, y1);

            renderer->DrawTriangle(p00, p10, p11);
            renderer->DrawTriangle(p11, p01, p00);
        }

        renderer->SetTopo(rhi::PrimitiveTopology::LINE_LIST);
        
        auto c0 = Color32{127, 0, 0, 128};
        auto c1 = Color32{31, 31, 0, 32};
        // Draw Section Grids.
        for (uint32_t i = 0; i < cfg.sectionNumX * size + 1; ++i) {
            auto x = static_cast<float>(i);
            auto  color = (i % size) == 0 ? c0 : c1;

            Vector3 p0 = worldPos + Vector3(x, 0, 0);
            Vector3 p1 = worldPos + Vector3(x, 0, boundY);

            renderer->SetColor(color);
            renderer->DrawLine(p0, p1);
        }
        for (uint32_t i = 0; i < cfg.sectionNumY * size + 1; ++i) {
            auto y = static_cast<float>(i);
            auto  color = (i % size) == 0 ? c0 : c1;

            Vector3 p0 = worldPos + Vector3(0,  0, y);
            Vector3 p1 = worldPos + Vector3(boundX, 0, y);

            renderer->SetColor(color);
            renderer->DrawLine(p0, p1);
        }

        renderer->Render(primitives);
    }

    void TerrainHelper::DrawTerrainBound(const TerrainData &data, const Vector3 &worldPos)
    {
        renderer->Reset();

        renderer->SetTopo(rhi::PrimitiveTopology::LINE_LIST);
        renderer->SetBlendEnable(true);
        renderer->SetColor(Color32{0, 127, 0, 128});

        auto size = ConvertSectionSize(data.sectionSize);

        float boundX = static_cast<float>(data.sectionBoundX) * static_cast<float>(size);
        float boundY = static_cast<float>(data.sectionBoundY) * static_cast<float>(size);
        // Draw Bounds
        {
            float x0 = 0;
            float y0 = 0;

            float x1 = boundX * data.resolution;
            float y1 = boundY * data.resolution;

            Vector3 p00 = worldPos + Vector3(x0, 0.f, y0);
            Vector3 p10 = worldPos + Vector3(x1, 0.f, y0);
            Vector3 p11 = worldPos + Vector3(x1, 0.f, y1);
            Vector3 p01 = worldPos + Vector3(x0, 0.f, y1);

            renderer->DrawLine(p00, p10);
            renderer->DrawLine(p10, p11);
            renderer->DrawLine(p11, p01);
            renderer->DrawLine(p01, p00);
        }

        renderer->Render(primitives);
    }

    void TerrainHelper::DrawSelectedGrid(const TerrainData &data, int32_t x, int32_t y, const Vector3 &worldPos)
    {
        renderer->Reset();

        renderer->SetTopo(rhi::PrimitiveTopology::TRIANGLE_LIST);
        renderer->SetBlendEnable(true);

        auto size = ConvertSectionSize(data.sectionSize);

        auto c0 = Color32{0,31,0,128};
        auto c1 = Color32{31, 31, 0, 128};

        for (const auto &terrain : data.sections) {
            const auto sectionX = terrain.coord.x;
            const auto sectionY = terrain.coord.y;

            auto color = (sectionX == x) && (sectionY == y) ? c1 : c0;

            renderer->SetColor(color);
            float x0 = static_cast<float>((sectionX + 0) * size) * data.resolution;
            float x1 = static_cast<float>((sectionX + 1) * size) * data.resolution;
            float y0 = static_cast<float>((sectionY + 0) * size) * data.resolution;
            float y1 = static_cast<float>((sectionY + 1) * size) * data.resolution;

            Vector3 p00 = worldPos + Vector3(x0, 0.f, y0);
            Vector3 p10 = worldPos + Vector3(x1, 0.f, y0);
            Vector3 p11 = worldPos + Vector3(x1, 0.f, y1);
            Vector3 p01 = worldPos + Vector3(x0, 0.f, y1);

            renderer->DrawTriangle(p00, p10, p11);
            renderer->DrawTriangle(p11, p01, p00);
        }

        // Draw Section Grid.
        renderer->SetTopo(rhi::PrimitiveTopology::LINE_LIST);
        renderer->SetColor(Color32{127, 0, 0, 128});
        for (const auto &terrain : data.sections) {
            const auto sectionX = terrain.coord.x;
            const auto sectionY = terrain.coord.y;

            float x0 = static_cast<float>((sectionX + 0)* size) * data.resolution;
            float x1 = static_cast<float>((sectionX + 1)* size) * data.resolution;
            float y0 = static_cast<float>((sectionY + 0)* size) * data.resolution;
            float y1 = static_cast<float>((sectionY + 1)* size) * data.resolution;

            Vector3 p00 = worldPos + Vector3(x0, 0.f, y0);
            Vector3 p10 = worldPos + Vector3(x1, 0.f, y0);
            Vector3 p11 = worldPos + Vector3(x1, 0.f, y1);
            Vector3 p01 = worldPos + Vector3(x0, 0.f, y1);

            renderer->DrawLine(p00, p10);
            renderer->DrawLine(p10, p11);
            renderer->DrawLine(p11, p01);
            renderer->DrawLine(p01, p00);
        }

        renderer->Render(primitives);
    }


} // namespace sky::editor
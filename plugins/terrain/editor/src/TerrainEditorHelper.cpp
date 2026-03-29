//
// Created by blues on 2024/11/30.
//

#include <terrain/editor/TerrainToolHelper.h>
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

    void TerrainHelper::DrawFullTerrainGrid(const ClipmapConfig &cfg, uint32_t tileCountX, uint32_t tileCountY, const Vector3 &worldPos)
    {
        renderer->Reset();

        float tileWorldSize = static_cast<float>(cfg.blockSize) * cfg.resolution;
        float boundX = static_cast<float>(tileCountX) * tileWorldSize;
        float boundY = static_cast<float>(tileCountY) * tileWorldSize;

        renderer->SetTopo(rhi::PrimitiveTopology::TRIANGLE_LIST);
        renderer->SetBlendEnable(true);
        renderer->SetColor(Color32{0, 32, 0, 128});
        {
            Vector3 p00 = worldPos;
            Vector3 p10 = worldPos + Vector3(boundX, 0.f, 0.f);
            Vector3 p11 = worldPos + Vector3(boundX, 0.f, boundY);
            Vector3 p01 = worldPos + Vector3(0.f, 0.f, boundY);
            renderer->DrawTriangle(p00, p10, p11);
            renderer->DrawTriangle(p11, p01, p00);
        }

        renderer->SetTopo(rhi::PrimitiveTopology::LINE_LIST);
        auto c0 = Color32{127, 0, 0, 128};
        // Draw tile grid lines
        for (uint32_t i = 0; i <= tileCountX; ++i) {
            float x = static_cast<float>(i) * tileWorldSize;
            renderer->SetColor(c0);
            renderer->DrawLine(worldPos + Vector3(x, 0, 0), worldPos + Vector3(x, 0, boundY));
        }
        for (uint32_t i = 0; i <= tileCountY; ++i) {
            float y = static_cast<float>(i) * tileWorldSize;
            renderer->SetColor(c0);
            renderer->DrawLine(worldPos + Vector3(0, 0, y), worldPos + Vector3(boundX, 0, y));
        }

        renderer->Render(primitives);
    }

    void TerrainHelper::DrawTerrainBound(const TerrainData &data, const Vector3 &worldPos)
    {
        renderer->Reset();

        renderer->SetTopo(rhi::PrimitiveTopology::LINE_LIST);
        renderer->SetBlendEnable(true);
        renderer->SetColor(Color32{0, 127, 0, 128});

        float tileWorldSize = static_cast<float>(data.config.blockSize) * data.config.resolution;
        float boundX = static_cast<float>(data.tileCountX) * tileWorldSize;
        float boundY = static_cast<float>(data.tileCountY) * tileWorldSize;

        Vector3 p00 = worldPos;
        Vector3 p10 = worldPos + Vector3(boundX, 0.f, 0.f);
        Vector3 p11 = worldPos + Vector3(boundX, 0.f, boundY);
        Vector3 p01 = worldPos + Vector3(0.f, 0.f, boundY);

        renderer->DrawLine(p00, p10);
        renderer->DrawLine(p10, p11);
        renderer->DrawLine(p11, p01);
        renderer->DrawLine(p01, p00);

        renderer->Render(primitives);
    }

    void TerrainHelper::DrawSelectedTile(const TerrainData &data, int32_t x, int32_t y, const Vector3 &worldPos)
    {
        renderer->Reset();

        float tileWorldSize = static_cast<float>(data.config.blockSize) * data.config.resolution;

        renderer->SetTopo(rhi::PrimitiveTopology::TRIANGLE_LIST);
        renderer->SetBlendEnable(true);

        auto c0 = Color32{0, 31, 0, 128};
        auto c1 = Color32{31, 31, 0, 128};

        // Highlight existing tiles
        for (uint32_t ty = 0; ty < data.tileCountY; ++ty) {
            for (uint32_t tx = 0; tx < data.tileCountX; ++tx) {
                auto color = (static_cast<int32_t>(tx) == x && static_cast<int32_t>(ty) == y) ? c1 : c0;
                renderer->SetColor(color);

                float x0 = static_cast<float>(tx) * tileWorldSize;
                float x1 = static_cast<float>(tx + 1) * tileWorldSize;
                float y0 = static_cast<float>(ty) * tileWorldSize;
                float y1 = static_cast<float>(ty + 1) * tileWorldSize;

                Vector3 p00 = worldPos + Vector3(x0, 0.f, y0);
                Vector3 p10 = worldPos + Vector3(x1, 0.f, y0);
                Vector3 p11 = worldPos + Vector3(x1, 0.f, y1);
                Vector3 p01 = worldPos + Vector3(x0, 0.f, y1);

                renderer->DrawTriangle(p00, p10, p11);
                renderer->DrawTriangle(p11, p01, p00);
            }
        }

        // Draw tile grid lines
        renderer->SetTopo(rhi::PrimitiveTopology::LINE_LIST);
        renderer->SetColor(Color32{127, 0, 0, 128});
        float boundX = static_cast<float>(data.tileCountX) * tileWorldSize;
        float boundY = static_cast<float>(data.tileCountY) * tileWorldSize;
        for (uint32_t i = 0; i <= data.tileCountX; ++i) {
            float xp = static_cast<float>(i) * tileWorldSize;
            renderer->DrawLine(worldPos + Vector3(xp, 0, 0), worldPos + Vector3(xp, 0, boundY));
        }
        for (uint32_t i = 0; i <= data.tileCountY; ++i) {
            float yp = static_cast<float>(i) * tileWorldSize;
            renderer->DrawLine(worldPos + Vector3(0, 0, yp), worldPos + Vector3(boundX, 0, yp));
        }

        renderer->Render(primitives);
    }


} // namespace sky::editor
//
// Created by blues on 2024/9/21.
//

#include <render/debug/Grid.h>
#include <core/math/MathUtil.h>

namespace sky {

    struct GridPrimitive : RenderPrimitive {
        void UpdateBatch() override {}
    };

    Grid::Grid()
        : renderer(new DebugRenderer())
        , primitive(new GridPrimitive())
    {
    }

    // from ImGuizmo
    void Grid::Draw(float gridSize)
    {
        renderer->Reset();

        for (float f = -gridSize; f <= gridSize; f += 1.f) {

            for (int i = 0; i < 2; ++i) {

                Vector3 pA = Vector3(i != 0 ? -gridSize : f, 0.f, i != 0 ? f : -gridSize);
                Vector3 pB = Vector3(i != 0 ?  gridSize : f, 0.f, i != 0 ? f :  gridSize);

                static constexpr Color32 color0(0x40, 0x40, 0x40, 0xFF);
                static constexpr Color32 color1(0x90, 0x90, 0x90, 0xFF);

                static constexpr Color32 colorR(0xFF, 0x00, 0x00, 0xFF);
                static constexpr Color32 colorG(0x00, 0xFF, 0x00, 0xFF);

                bool mode1 = std::fmodf(std::fabsf(f), 10.f) < FLT_EPSILON;
                bool modeR = std::fabsf(pA.x) < FLT_EPSILON && std::fabsf(pB.x) < FLT_EPSILON;
                bool modeG = std::fabsf(pA.z) < FLT_EPSILON && std::fabsf(pB.z) < FLT_EPSILON;

                Color32 color = mode1 ? color1 : color0;
                color = modeR ? colorR : color;
                color = modeG ? colorG : color;

                renderer->SetColor(color);
                renderer->DrawLine(pA, pB);
            }
        }

        renderer->Render(primitive.get());
    }

    void Grid::SetTechnique(const RDGfxTechPtr &tech)
    {
        RenderBatch techInst = {tech};
        techInst.topo = rhi::PrimitiveTopology::LINE_LIST;
        primitive->batches.clear();
        primitive->batches.emplace_back(techInst);
    }
} // namespace sky
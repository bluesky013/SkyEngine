//
// Created on 2026/09/23.
//

#include <vegetation/VegetationRenderAdaptor.h>
#include <vegetation/VegetationSystem.h>

#include <core/async/Task.h>

#include <gtest/gtest.h>

using namespace sky;
using namespace sky::vegetation;

namespace {

    class UniformSurface : public IVegetationSurfaceProvider {
    public:
        float cellSize = 8.f;

        bool SampleSurface(const Vector3 &, VegetationSurfaceSample &out) const override
        {
            out.height          = 0.f;
            out.normal          = VEC3_Y;
            out.layerWeights[0] = 1.f;
            out.valid           = true;
            return true;
        }
        float GetCellSize() const override { return cellSize; }
        void  GetCellRange(const AABB &bounds, int32_t &minX, int32_t &minY, int32_t &maxX, int32_t &maxY) const override
        {
            minX = static_cast<int32_t>(std::floor(bounds.min.x / cellSize));
            minY = static_cast<int32_t>(std::floor(bounds.min.z / cellSize));
            maxX = static_cast<int32_t>(std::floor(bounds.max.x / cellSize));
            maxY = static_cast<int32_t>(std::floor(bounds.max.z / cellSize));
        }
        AABB GetCellBounds(int32_t cellX, int32_t cellY) const override
        {
            const float x0 = static_cast<float>(cellX) * cellSize;
            const float y0 = static_cast<float>(cellY) * cellSize;
            return AABB(Vector3(x0, -1000.f, y0), Vector3(x0 + cellSize, 1000.f, y0 + cellSize));
        }
    };

    VegetationPalette MakePalette()
    {
        VegetationBiome biome;
        biome.id          = 1;
        biome.maxSlopeDeg = 90.f;
        biome.layerIndex  = -1;
        biome.density     = 1.f;
        biome.species.push_back(VegetationSpecies{});
        return VegetationPalette{{biome}};
    }

    class FakeAdaptor : public IVegetationRenderAdaptor {
    public:
        void OnCellLoaded(const VegetationRenderCell &cell) override
        {
            ++loaded;
            lastInstances = cell.instances.size();
        }
        void OnCellUnloaded(int32_t, int32_t) override { ++unloaded; }

        uint32_t loaded   = 0;
        uint32_t unloaded = 0;
        size_t   lastInstances = 0;
    };

    class FakeFactory : public VegetationRenderFactory::Impl {
    public:
        FakeFactory() { adaptor = new FakeAdaptor(); }
        IVegetationRenderAdaptor *CreateAdaptor() override { return adaptor; }
        FakeAdaptor *GetAdaptor() const { return adaptor; }

    private:
        FakeAdaptor *adaptor = nullptr;
    };

    void Settle(VegetationSystem &system)
    {
        for (int i = 0; i < 64; ++i) {
            system.Tick(0.f);
            TaskExecutor::Get()->WaitForAll();
            system.Tick(0.f);
            if (system.GetPendingCellCount() == 0 && system.GetLoadedCellCount() > 0) {
                break;
            }
        }
    }

} // namespace

TEST(VegetationRenderAdaptorTest, PushesCellEventsToAdaptor)
{
    auto *factory = new FakeFactory();
    VegetationRenderFactory::Get()->Register(factory);
    FakeAdaptor *adaptor = factory->GetAdaptor();

    UniformSurface surface;
    {
        VegetationSystem system;
        system.SetSurfaceProvider(&surface);
        system.SetPalette(MakePalette());
        system.SetStreamingEnabled(true);
        system.SetStreamingFocus(Vector3(4.f, 0.f, 4.f));
        system.SetStreamingRadii(15.f, 23.f);
        system.SetLoadBudget(100);
        Settle(system);

        EXPECT_GT(adaptor->loaded, 0u);
        EXPECT_GT(adaptor->lastInstances, 0u);

        const uint32_t loadedBefore = adaptor->loaded;
        system.NotifyRegionChanged(AABB(Vector3(0.f, -1000.f, 0.f), Vector3(8.f, 1000.f, 8.f)));
        EXPECT_GT(adaptor->unloaded, 0u);
        EXPECT_EQ(adaptor->loaded, loadedBefore);
    }

    VegetationRenderFactory::Get()->UnRegister();
}

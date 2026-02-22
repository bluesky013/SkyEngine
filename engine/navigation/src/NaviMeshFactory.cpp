//
// Created by blues on 2024/10/11.
//

#include <navigation/NaviMeshFactory.h>
#include <navigation/NavigationSystem.h>
#include <framework/interface/Interface.h>

namespace sky::ai {

    class NavigationBuilder : public IWorldBuilder {
    public:
        explicit NavigationBuilder() = default;
        ~NavigationBuilder() override = default;

        void Build(const WorldPtr &world) const override
        {
            CounterPtr<NaviMeshGenerator> generator = NaviMeshFactory::Get()->CreateGenerator();
            generator->Setup(world);
            generator->StartAsync();
        }

        std::string GetDesc() const override
        {
            return "Build Navigation";
        }
    };

    NaviMeshFactory::NaviMeshFactory()
    {
        binder.Bind(this);
    }

    void NaviMeshFactory::Register(Impl* impl)
    {
        factory.reset(impl);
    }

    CounterPtr<NaviMesh> NaviMeshFactory::CreateNaviMesh()
    {
        return factory ? factory->CreateNaviMesh() : nullptr;
    }

    CounterPtr<NaviMeshGenerator> NaviMeshFactory::CreateGenerator()
    {
        return factory ? factory->CreateGenerator() : nullptr;
    }

    void NaviMeshFactory::Gather(std::list<CounterPtr<IWorldBuilder>> &builders) const
    {
        if (factory) {
            builders.emplace_back(new NavigationBuilder());
        }
    }

} // namespace sky::ai
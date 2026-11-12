#pragma once

#include <framework/world/Component.h>
#include <physics/softbody/SoftBodyTypes.h>
#include <softbody/assets/SoftBodyComponentData.h>

#include <memory>

namespace sky::softbody {
    bool StartSoftBodyWorldControl();
    void StopSoftBodyWorldControl();

    class SoftBodyComponent final : public ComponentAdaptor<SoftBodyComponentData> {
    public:
        SoftBodyComponent();
        ~SoftBodyComponent() override;
        static void Reflect(SerializationContext *context);
        COMPONENT_RUNTIME_INFO(SoftBodyComponent)

        bool        SetRig(const Uuid &rig);
        const Uuid &GetRig() const
        {
            return data.rig;
        }
        phy::PhysicsStatus   GetLastStatus() const;
        phy::PhysicsStatus   Reset();
        phy::PhysicsStatus   GetSnapshot(phy::SoftBodySnapshot &out) const;
        void                 CollectAssetReferences(std::vector<ComponentAssetReference> &out) const override;
        ComponentPreparation PrepareForWorld(World &world, std::string &error) override;
        ComponentPreparation PollWorldPreparation(std::string &error) override;
        void                 CancelPreparation() override;
        bool                 KeepWorldResidency() const override
        {
            return true;
        }

    private:
        bool TryAttachToWorld(std::string &error) override;
        void OnDetachFromWorld() override;
        void Tick(float time) override;
        void PostTick(float time) override;
        struct State;
        struct Resources;
        std::shared_ptr<State>     state;
        std::unique_ptr<Resources> resources;
        phy::PhysicsStatus         status;
    };
} // namespace sky::softbody

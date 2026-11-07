#pragma once

#include <framework/asset/Asset.h>
#include <framework/world/World.h>
#include <particle/ParticleEvents.h>
#include <particle/ParticleRenderTypes.h>
#include <particle/ParticleSolver.h>
#include <particle/world/ParticleComponent.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace sky::particle {

    class ParticleEffectAsset;
    struct ParticleViewInfo;

    struct ParticleWorldConfig {
        uint32_t maxParticles = 65536;
        bool     simulate     = true;
        float    lodStart     = 30.f;
        float    lodEnd       = 120.f;
        float    lodMinRate   = 0.1f;
        float    cullDistance = 250.f;
    };

    bool ValidateParticleWorldConfig(const ParticleWorldConfig &, std::string &error);

    class ParticleSystem final : public IWorldSubSystem {
    public:
        static constexpr std::string_view NAME = "Particle";

        void OnAttachToWorld(World &world) override;
        void OnDetachFromWorld(World &world) override;
        void StartSimulation() override;
        void StopSimulation() override;
        void Tick(float time) override;

        uint32_t GetActiveParticleCount() const;
        bool     SetConfig(const ParticleWorldConfig &value);
        const std::string         &GetLastError() const;

    private:
        struct EmitterState {
            ParticleSolver solver;
        };
        struct Instance {
            Uuid                                        attachment;
            ParticleComponentData                       config;
            std::shared_ptr<Asset<ParticleEffectAsset>> asset;
            std::vector<EmitterState>                   emitters;
            Uuid                                        productInstance;
            uint64_t                                    previewRevision = 0;
            Matrix4                                     transform = Matrix4::Identity();
            std::string                                 error;
            bool                                        pending   = false;
            bool                                        seen      = false;
            bool                                        culled    = false;
            bool                                        previewActive = false;
            float                                       rateScale = 1.f;
        };

        void Reconcile(World &world);
        void RequestInstance(Instance &instance);
        void PollInstance(Instance &instance);
        bool ApplyPrograms(Instance &instance, const std::vector<ParticleProgram> &programs);
        void Simulate(const ParticleViewInfo &view, float time);
        void SubmitEvents();
        std::shared_ptr<ParticleRenderFrame> BuildFrame(const World &world) const;
        void Publish(World &world);

        World                             *world = nullptr;
        ParticleWorldConfig                config;
        bool                               running     = false;
        bool                               sinkAttached = false;
        uint64_t                           revision    = 0;
        uint64_t                           assetSerial = 0;
        std::string                        lastError;
        std::vector<Uuid>                  activeInstances;
        std::vector<ParticleEvent>         eventScratch;
        std::unordered_map<Uuid, Instance> instances;
    };

} // namespace sky::particle

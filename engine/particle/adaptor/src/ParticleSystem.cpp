#include <particle/world/ParticleSystem.h>

#include <particle/assets/ParticlePreviewEffectRegistry.h>

#include <framework/asset/AssetManager.h>
#include <framework/interface/Interface.h>
#include <framework/world/TransformComponent.h>
#include <particle/ParticleSchema.h>
#include <particle/ParticleView.h>
#include <particle/assets/ParticlePreviewEffectRegistry.h>

#include <algorithm>
#include <cmath>

namespace sky::particle {
    namespace {
        uint32_t SeedFrom(const Uuid &id)
        {
            uint32_t hash = 2166136261u;
            for (const auto word : id.u32) {
                hash ^= word;
                hash *= 16777619u;
            }
            return hash ? hash : 1u;
        }

        Vector3 TransformPosition(const Matrix4 &matrix, const Vector3 &position)
        {
            const auto value = matrix * Vector4{position.x, position.y, position.z, 1.f};
            return {value.x, value.y, value.z};
        }
    } // namespace

    bool ValidateParticleWorldConfig(const ParticleWorldConfig &value, std::string &error)
    {
        if (value.maxParticles == 0 || value.maxParticles > MAX_PARTICLE_CAPACITY) {
            error = "Particle maxParticles must be between 1 and 16777216";
            return false;
        }
        if (!std::isfinite(value.lodStart) || !std::isfinite(value.lodEnd) || !std::isfinite(value.lodMinRate) ||
            !std::isfinite(value.cullDistance) || value.lodStart < 0.f || value.lodEnd < value.lodStart || value.lodMinRate < 0.f ||
            value.lodMinRate > 1.f || value.cullDistance < value.lodEnd) {
            error = "Particle distance LOD requires finite ordered distances and a rate between zero and one";
            return false;
        }
        error.clear();
        return true;
    }

    void ParticleSystem::OnAttachToWorld(World &inWorld)
    {
        world   = &inWorld;
        running = false;
        instances.clear();
        activeInstances.clear();
        assetSerial = AssetManager::Get()->GetProductChangeSerial();
    }

    void ParticleSystem::OnDetachFromWorld(World &inWorld)
    {
        if (sinkAttached) {
            if (auto *sink = Interface<IParticleRenderSink>::Get()->GetApi())
                sink->RemoveWorld(inWorld.GetRuntimeId());
            sinkAttached = false;
        }
        instances.clear();
        activeInstances.clear();
        world = nullptr;
    }

    void ParticleSystem::StartSimulation()
    {
        running = true;
    }

    void ParticleSystem::StopSimulation()
    {
        running = false;
        if (world && sinkAttached) {
            if (auto *sink = Interface<IParticleRenderSink>::Get()->GetApi())
                sink->RemoveWorld(world->GetRuntimeId());
            sinkAttached = false;
        }
        instances.clear();
        activeInstances.clear();
    }

    bool ParticleSystem::SetConfig(const ParticleWorldConfig &value)
    {
        if (!ValidateParticleWorldConfig(value, lastError))
            return false;
        config = value;
        return true;
    }

    const std::string &ParticleSystem::GetLastError() const
    {
        return lastError;
    }

    void ParticleSystem::Tick(float time)
    {
        if (!world)
            return;
        if (!running || !config.simulate) {
            if (sinkAttached) {
                if (auto *sink = Interface<IParticleRenderSink>::Get()->GetApi())
                    sink->RemoveWorld(world->GetRuntimeId());
                sinkAttached = false;
            }
            return;
        }
        Reconcile(*world);
        ParticleViewInfo view;
        if (auto *provider = Interface<IParticleViewProvider>::Get()->GetApi())
            provider->GetView(world->GetRuntimeId(), view);
        Simulate(view, time);
        SubmitEvents();
        Publish(*world);
    }

    void ParticleSystem::Simulate(const ParticleViewInfo &view, float time)
    {
        uint32_t remaining = config.maxParticles;
        for (const auto &id : activeInstances) {
            auto &instance     = instances.at(id);
            instance.culled    = false;
            instance.rateScale = 1.f;
            if (view.valid) {
                const auto position = TransformPosition(instance.transform, {});
                const auto distance = (position - view.position).Length();
                instance.culled     = distance >= config.cullDistance;
                if (!instance.culled && distance > config.lodStart) {
                    const auto range   = std::max(1.f, config.lodEnd - config.lodStart);
                    const auto t       = std::min(1.f, (distance - config.lodStart) / range);
                    instance.rateScale = config.lodMinRate + (1.f - config.lodMinRate) * (1.f - t);
                }
            }
            auto allowed = instance.config.budget ? std::min(remaining, instance.config.budget) : remaining;
            for (auto &emitter : instance.emitters) {
                emitter.solver.SetSpawnTransform(instance.transform);
                emitter.solver.SetRateScale(instance.config.rateScale * instance.rateScale);
                if (!instance.culled && instance.config.autoplay)
                    emitter.solver.Step(time);
                emitter.solver.KillExcess(allowed);
                const auto alive = emitter.solver.GetStorage().aliveCount;
                allowed -= alive;
                remaining -= alive;
            }
        }
    }

    void ParticleSystem::SubmitEvents()
    {
        auto *sink = Interface<IParticleEventSink>::Get()->GetApi();
        if (!sink)
            return;
        eventScratch.clear();
        for (const auto &id : activeInstances)
            for (auto &emitter : instances.at(id).emitters)
                emitter.solver.DrainEvents(eventScratch);
        if (!eventScratch.empty())
            sink->Submit(eventScratch);
    }

    uint32_t ParticleSystem::GetActiveParticleCount() const
    {
        uint32_t count = 0;
        for (const auto &[key, instance] : instances)
            for (const auto &emitter : instance.emitters)
                count += emitter.solver.GetStorage().aliveCount;
        return count;
    }

    void ParticleSystem::RequestInstance(Instance &instance)
    {
        instance.error.clear();
        if (ParticlePreviewEffectRegistry::Get()->Find(world->GetRuntimeId(), instance.config.effect)) {
            instance.asset.reset();
            instance.pending = false;
            return;
        }
        instance.asset   = AssetManager::Get()->LoadAsset<ParticleEffectAsset>(instance.config.effect, AssetLoadPolicy::ProductsOnly);
        instance.pending = instance.asset != nullptr;
        if (!instance.asset) {
            instance.emitters.clear();
            instance.error = "Particle effect product is unavailable";
        }
    }

    void ParticleSystem::PollInstance(Instance &instance)
    {
        if (auto *preview = ParticlePreviewEffectRegistry::Get(); preview) {
            const auto data = preview->Find(world->GetRuntimeId(), instance.config.effect);
            if (data) {
                if (instance.previewRevision == preview->GetRevision())
                    return;
                instance.previewRevision = preview->GetRevision();
                if (!data->valid || data->programs.empty()) {
                    instance.emitters.clear();
                    instance.error = "Particle effect preview data is invalid";
                    return;
                }
                if (ApplyPrograms(instance, data->programs))
                    instance.error.clear();
                return;
            }
        }
        if (!instance.pending)
            return;
        if (instance.asset->GetStatus() == AssetBase::Status::FAILED) {
            instance.pending = false;
            instance.error   = "Particle effect product failed to load";
            return;
        }
        if (!instance.asset->IsLoaded())
            return;
        instance.pending = false;
        if (!instance.asset->Data().IsLoadValid()) {
            instance.error = "Particle effect product is invalid";
            return;
        }
        if (instance.productInstance == instance.asset->GetInstanceId())
            return;
        ApplyPrograms(instance, instance.asset->Data().programs);
        instance.productInstance = instance.asset->GetInstanceId();
    }

    bool ParticleSystem::ApplyPrograms(Instance &instance, const std::vector<ParticleProgram> &programs)
    {
        const bool unchanged = instance.emitters.size() == programs.size() &&
                               std::equal(instance.emitters.begin(), instance.emitters.end(), programs.begin(),
                                          [](const auto &emitter, const auto &program) { return emitter.solver.GetProgram().hash == program.hash; });
        if (unchanged)
            return true;
        std::vector<EmitterState> emitters;
        emitters.reserve(programs.size());
        uint32_t index = 0;
        for (const auto &program : programs) {
            EmitterState       state;
            ParticleSolverDesc desc;
            desc.randomSeed = SeedFrom(instance.attachment) + index++;
            desc.rateScale  = instance.config.rateScale;
            if (!state.solver.Init(program, desc, instance.transform)) {
                instance.error = "Particle emitter initialization failed";
                return false;
            }
            emitters.push_back(std::move(state));
        }
        instance.emitters = std::move(emitters);
        return true;
    }

    void ParticleSystem::Reconcile(World &inWorld)
    {
        const auto nextSerial = AssetManager::Get()->GetProductChangeSerial();
        const bool reload     = nextSerial != assetSerial;
        assetSerial           = nextSerial;
        lastError.clear();
        activeInstances.clear();
        for (auto &[key, instance] : instances)
            instance.seen = false;
        for (const auto &actor : inWorld.GetActors()) {
            const auto *component = actor->GetComponent<ParticleComponent>();
            if (!component || !component->GetData().effect)
                continue;
            const auto &data = component->GetData();
            if (!std::isfinite(data.rateScale) || data.rateScale < 0.f) {
                lastError = "Particle component rate scale must be finite and nonnegative";
                continue;
            }
            auto      &instance = instances[actor->GetUuid()];
            const bool changed  = instance.attachment != component->GetAttachment() || instance.config.effect != data.effect;
            if (changed) {
                instance            = {};
                instance.attachment = component->GetAttachment();
            }
            instance.seen         = true;
            instance.config       = data;
            const auto *transform = actor->GetComponent<TransformComponent>();
            instance.transform    = transform ? transform->GetWorldMatrix() : Matrix4::Identity();
            const bool preview = ParticlePreviewEffectRegistry::Get()->Find(inWorld.GetRuntimeId(), instance.config.effect) != nullptr;
            if (changed || reload || instance.previewActive != preview) {
                instance.previewActive = preview;
                RequestInstance(instance);
            }
            PollInstance(instance);
            if (!instance.error.empty())
                lastError = instance.error;
            activeInstances.push_back(actor->GetUuid());
        }
        std::erase_if(instances, [](const auto &entry) { return !entry.second.seen; });
    }

    std::shared_ptr<ParticleRenderFrame> ParticleSystem::BuildFrame(const World &inWorld) const
    {
        auto frame      = std::make_shared<ParticleRenderFrame>();
        frame->world    = inWorld.GetRuntimeId();
        for (const auto &id : activeInstances)
            frame->emitters.reserve(frame->emitters.size() + instances.at(id).emitters.size());
        for (const auto &id : activeInstances) {
            const auto &instance = instances.at(id);
            if (!instance.config.visible || instance.culled)
                continue;
            for (const auto &emitter : instance.emitters) {
                const auto &program = emitter.solver.GetProgram();
                const auto &storage = emitter.solver.GetStorage();
                const auto  subUv   = std::find_if(program.updateModules.begin(), program.updateModules.end(),
                                                   [](const auto &module) { return module.type == ParticleModuleType::SUB_UV; });
                const auto  columns = subUv == program.updateModules.end() ? 1u : static_cast<uint32_t>(subUv->values[0]);
                const auto  rows    = subUv == program.updateModules.end() ? 1u : static_cast<uint32_t>(subUv->values[1]);
                for (const auto &renderer : program.renderers) {
                    ParticleRenderEmitter out;
                    out.programHash = program.hash;
                    out.texture     = renderer.texture;
                    out.type        = renderer.type;
                    out.blend       = renderer.blend;
                    out.instances.reserve(storage.aliveCount);
                    for (const auto slot : storage.alive) {
                        ParticleRenderInstance particle;
                        particle.position  = program.space == ParticleSimSpace::LOCAL ? TransformPosition(instance.transform, storage.positions[slot])
                                                                                      : storage.positions[slot];
                        particle.size      = storage.sizes[slot];
                        particle.rotation  = storage.rotations[slot];
                        particle.color     = storage.colors[slot];
                        const auto cell    = static_cast<uint32_t>(std::clamp(double(storage.subUv[slot]), 0.0, double(columns) * rows - 1.0));
                        particle.subU      = float(cell % columns) / float(columns);
                        particle.subV      = float(cell / columns) / float(rows);
                        particle.subWidth  = 1.f / float(columns);
                        particle.subHeight = 1.f / float(rows);
                        out.instances.push_back(particle);
                    }
                    frame->emitters.push_back(std::move(out));
                }
            }
        }
        return frame;
    }

    void ParticleSystem::Publish(World &inWorld)
    {
        auto *sink = Interface<IParticleRenderSink>::Get()->GetApi();
        if (!sink)
            return;
        auto frame = BuildFrame(inWorld);
        frame->revision = ++revision;
        sinkAttached = true;
        sink->Submit(std::move(frame));
    }

} // namespace sky::particle

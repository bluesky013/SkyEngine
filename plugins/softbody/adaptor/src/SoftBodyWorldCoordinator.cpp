#include "SoftBodyWorldCoordinator.h"

#include <core/environment/Singleton.h>
#include <framework/interface/Interface.h>
#include <physics/IPhysicsSystem.h>
#include <physics/softbody/ISoftBodyCollisionQuery.h>
#include <physics/softbody/ISoftBodyWorldControl.h>
#include <softbody/solver/CpuSoftBodySolver.h>
#include <softbody/world/SoftBodyComponent.h>

#include <algorithm>
#include <cmath>
#include <deque>
#include <unordered_map>

namespace sky::softbody {
    namespace world_runtime {
        using namespace phy;
        constexpr size_t SAMPLE_CAPACITY = 4096;

        PhysicsStatus Unavailable()
        {
            return {PhysicsError::InvalidState, "Soft body World session is unavailable"};
        }

        class Coordinator;
        void Retire(const Uuid &world, const Coordinator *owner);

        class Coordinator final : public SoftBodyWorldRegistration, public IPhysicsStepListener {
        public:
            Coordinator(Uuid world, IPhysicsSystem &host) : world(world), host(&host), physics(host.GetWorld())
            {
            }

            bool Matches(IPhysicsSystem &value) const
            {
                return host == &value && physics && physics == value.GetWorld();
            }
            bool Live() const
            {
                return host && physics;
            }
            PhysicsStatus Add(const std::shared_ptr<SoftBodyWorldMember> &member)
            {
                if (!Live() || !member)
                    return Unavailable();
                const auto key = member->GetOrderKey();
                if (!key)
                    return {PhysicsError::InvalidArgument, "Soft body member requires a stable actor identity"};
                for (const auto &entry : members) {
                    const auto value = entry.lock();
                    if (value == member)
                        return {PhysicsError::InvalidArgument, "Soft body member is already registered"};
                    if (value && value->GetOrderKey() == key)
                        return {PhysicsError::InvalidArgument, "Soft body actor identity is already registered"};
                }
                members.push_back(member);
                return {};
            }
            void Remove(const SoftBodyWorldMember *member) override
            {
                std::erase_if(members, [&](const auto &entry) {
                    const auto value = entry.lock();
                    return !value || value.get() == member;
                });
                std::erase_if(pending, [&](const auto &value) { return value.get() == member; });
                if (members.empty())
                    Stop();
            }
            void Stop()
            {
                auto *oldHost    = host;
                auto *oldWorld   = physics;
                host             = nullptr;
                physics          = nullptr;
                const auto owned = LockMembers();
                members.clear();
                pending.clear();
                if (oldWorld)
                    for (const auto &member : owned)
                        member->Close(*oldWorld);
                if (oldHost)
                    oldHost->RemoveStepListener(this);
                Retire(world, this);
            }
            PhysicsStatus Configure(const SoftBodySimulationSettings &value)
            {
                if (!Live())
                    return Unavailable();
                if (!std::isfinite(value.softContactCompliance) || value.softContactCompliance < 0.f)
                    return {PhysicsError::InvalidArgument, "Soft contact compliance must be finite and nonnegative"};
                settings = value;
                return {};
            }
            SoftBodyStepMetrics Stats() const
            {
                auto result      = last;
                result.bodyCount = 0;
                for (const auto &member : LockMembers())
                    result.bodyCount += member->GetSolver() != nullptr;
                return result;
            }
            PhysicsStatus Read(uint64_t afterStep, std::vector<SoftBodyStepMetrics> &out) const
            {
                if (afterStep < discardedThrough)
                    return {PhysicsError::CapacityExceeded, "Soft body profiling cursor expired from the bounded sample history"};
                std::vector<SoftBodyStepMetrics> result;
                for (const auto &sample : samples)
                    if (sample.step > afterStep)
                        result.push_back(sample);
                out = std::move(result);
                return {};
            }
            PhysicsStatus ReadDebug(std::vector<SoftBodyDebugBody> &out) const
            {
                auto owned = LockMembers();
                std::sort(owned.begin(), owned.end(), [](const auto &a, const auto &b) { return a->GetOrderKey() < b->GetOrderKey(); });
                std::vector<SoftBodyDebugBody> result;
                for (const auto &member : owned) {
                    if (!member->GetSolver())
                        continue;
                    SoftBodyDebugBody body;
                    if (const auto status = member->ReadDebugBody(body); !status)
                        return status;
                    result.push_back(std::move(body));
                }
                out = std::move(result);
                return {};
            }
            PhysicsStatus ReadParameters(std::vector<SoftBodyRuntimeParameters> &out) const
            {
                std::vector<SoftBodyRuntimeParameters> result;
                for (const auto &member : LockMembers())
                    if (auto *solver = member->GetSolver()) {
                        SoftBodyRuntimeParameters row;
                        if (const auto status = solver->ReadParameters(row); !status)
                            return status;
                        row.actor = member->GetOrderKey();
                        result.push_back(row);
                    }
                std::sort(result.begin(), result.end(), [](const auto &a, const auto &b) { return a.actor < b.actor; });
                out = std::move(result);
                return {};
            }
            PhysicsStatus ApplyParameters(std::span<const Uuid> ids, const SoftBodyParameterPatch &patch)
            {
                if (!pending.empty() || solving)
                    return {PhysicsError::InvalidState, "Soft body material edits require a fixed-step boundary"};
                const auto                       owned = LockMembers();
                std::vector<CpuSoftBodySolver *> selected;
                for (const auto &id : ids) {
                    auto it = std::find_if(owned.begin(), owned.end(), [&](const auto &member) {
                        const auto *solver = member->GetSolver();
                        return solver && solver->GetId() == id;
                    });
                    if (it == owned.end())
                        return {PhysicsError::InvalidArgument, "Soft body tuning target is stale or belongs to another World"};
                    selected.push_back((*it)->GetSolver());
                }
                return CpuSoftBodySolver::ApplyParameters(selected, patch);
            }
            PhysicsStatus BeforePhysicsStep(IPhysicsWorld &stepping, float) override
            {
                pending.clear();
                if (!Live() || physics != &stepping)
                    return Unavailable();
                pending = LockMembers();
                std::sort(pending.begin(), pending.end(), [](const auto &a, const auto &b) { return a->GetOrderKey() < b->GetOrderKey(); });
                Uuid previous;
                for (const auto &member : pending) {
                    const auto key    = member->GetOrderKey();
                    const auto status = !key || key == previous
                                            ? PhysicsStatus{PhysicsError::InvalidState, "Soft body actor ordering identity is stale or duplicated"}
                                            : member->UpdateRoot(stepping);
                    if (!status) {
                        for (const auto &other : pending)
                            other->Report(status);
                        pending.clear();
                        return status;
                    }
                    previous = key;
                }
                return {};
            }
            PhysicsStatus AfterPhysicsStep(IPhysicsWorld &stepping, float delta) override
            {
                if (!Live() || physics != &stepping) {
                    pending.clear();
                    return Unavailable();
                }
                auto                                              owned = std::move(pending);
                std::vector<std::shared_ptr<SoftBodyWorldMember>> active;
                std::vector<CpuSoftBodySolver *>                  solvers;
                for (const auto &member : owned)
                    if (auto *solver = member->GetSolver()) {
                        active.push_back(member);
                        solvers.push_back(solver);
                    }
                if (solvers.empty())
                    return {};
                const auto         *query  = physics->GetSoftBodyCollisionQuery();
                PhysicsStatus       status = query ? PhysicsStatus{} : Unavailable();
                SoftBodyStepMetrics metrics;
                metrics.step           = physics->GetStats().stepCount;
                metrics.attemptedSteps = 1;
                metrics.bodyCount      = solvers.size();
                if (status) {
                    for (size_t i = 0; i < solvers.size(); ++i) {
                        status = active[i]->ValidatePublication(solvers[i]->GetSnapshot());
                        if (!status)
                            break;
                    }
                }
                // Main-thread membership stays fixed through the batch; publication only moves owned snapshots.
                if (status) {
                    solving = true;
                    status  = CpuSoftBodySolver::StepBatch(solvers, delta, *query, metrics.step, settings, &metrics);
                    solving = false;
                }
                if (status)
                    for (size_t i = 0; i < active.size(); ++i)
                        active[i]->Publish(solvers[i]->GetSnapshot());
                for (const auto &member : active)
                    member->Report(status);
                if (!status) {
                    metrics.failure        = status.error;
                    metrics.committed      = false;
                    metrics.completedSteps = 0;
                }
                last = metrics;
                if (settings.profileSteps) {
                    if (samples.size() == SAMPLE_CAPACITY) {
                        discardedThrough = samples.front().step;
                        samples.pop_front();
                    }
                    samples.push_back(metrics);
                }
                return status;
            }
            void OnPhysicsWorldClosing(IPhysicsWorld &closing) override
            {
                if (physics == &closing)
                    Stop();
            }

        private:
            std::vector<std::shared_ptr<SoftBodyWorldMember>> LockMembers() const
            {
                std::vector<std::shared_ptr<SoftBodyWorldMember>> result;
                for (const auto &member : members)
                    if (const auto value = member.lock())
                        result.push_back(value);
                return result;
            }

            Uuid                                            world;
            IPhysicsSystem                                 *host    = nullptr;
            IPhysicsWorld                                  *physics = nullptr;
            std::vector<std::weak_ptr<SoftBodyWorldMember>> members;
            // Members retain only a weak registration, so a failed rigid step cannot form a cycle.
            std::vector<std::shared_ptr<SoftBodyWorldMember>> pending;
            bool                                              solving = false;
            SoftBodySimulationSettings                        settings;
            SoftBodyStepMetrics                               last;
            std::deque<SoftBodyStepMetrics>                   samples;
            uint64_t                                          discardedThrough = 0;
        };

        class Directory final : public Singleton<Directory>, public ISoftBodyWorldControl {
            friend class Singleton<Directory>;

        public:
            bool                                                 active = false;
            std::unordered_map<Uuid, std::weak_ptr<Coordinator>> worlds;

            std::shared_ptr<Coordinator> Find(const Uuid &id) const
            {
                const auto found = worlds.find(id);
                if (found == worlds.end())
                    return {};
                const auto value = found->second.lock();
                return value && value->Live() ? value : nullptr;
            }
            PhysicsStatus Configure(const Uuid &world, const SoftBodySimulationSettings &settings) override
            {
                const auto value = Find(world);
                return value ? value->Configure(settings) : Unavailable();
            }
            PhysicsStatus GetStats(const Uuid &world, SoftBodyStepMetrics &out) const override
            {
                const auto value = Find(world);
                if (!value)
                    return Unavailable();
                out = value->Stats();
                return {};
            }
            PhysicsStatus ReadSamples(const Uuid &world, uint64_t afterStep, std::vector<SoftBodyStepMetrics> &out) const override
            {
                const auto value = Find(world);
                return value ? value->Read(afterStep, out) : Unavailable();
            }
            PhysicsStatus ReadDebugBodies(const Uuid &world, std::vector<SoftBodyDebugBody> &out) const override
            {
                const auto value = Find(world);
                return value ? value->ReadDebug(out) : Unavailable();
            }
            PhysicsStatus ReadBodyParameters(const Uuid &world, std::vector<SoftBodyRuntimeParameters> &out) const override
            {
                const auto value = Find(world);
                return value ? value->ReadParameters(out) : Unavailable();
            }
            PhysicsStatus ApplyBodyParameters(const Uuid &world, std::span<const Uuid> bodyIds, const SoftBodyParameterPatch &patch) override
            {
                const auto value = Find(world);
                return value ? value->ApplyParameters(bodyIds, patch) : Unavailable();
            }
            void Drain()
            {
                std::vector<std::shared_ptr<Coordinator>> owned;
                for (const auto &[id, weak] : worlds)
                    if (const auto value = weak.lock())
                        owned.push_back(value);
                for (const auto &value : owned)
                    value->Stop();
                worlds.clear();
            }
        };

        void Retire(const Uuid &world, const Coordinator *owner)
        {
            auto      &worlds = Directory::Get()->worlds;
            const auto found  = worlds.find(world);
            if (found != worlds.end() && found->second.lock().get() == owner)
                worlds.erase(found);
        }
    } // namespace world_runtime

    using namespace world_runtime;

    phy::PhysicsStatus RegisterSoftBodyWorldMember(const Uuid                                 &world,
                                                   phy::IPhysicsSystem                        &physics,
                                                   const std::shared_ptr<SoftBodyWorldMember> &member,
                                                   std::shared_ptr<SoftBodyWorldRegistration> &out)
    {
        auto &directory = *Directory::Get();
        if (!directory.active || !world || !physics.GetWorld() || !member)
            return Unavailable();
        auto       value   = directory.Find(world);
        const bool created = !value;
        if (value && !value->Matches(physics))
            return {phy::PhysicsError::InvalidState, "Soft body World runtime ID belongs to a different physics owner"};
        if (!value) {
            value = std::make_shared<Coordinator>(world, physics);
            if (const auto result = physics.AddStepListener(value); !result)
                return result;
            directory.worlds[world] = value;
        }
        if (const auto result = value->Add(member); !result) {
            if (created)
                value->Stop();
            return result;
        }
        out = std::move(value);
        return {};
    }

    bool StartSoftBodyWorldControl()
    {
        auto *service   = Interface<phy::ISoftBodyWorldControl>::Get();
        auto &directory = *Directory::Get();
        if (service->GetApi() || directory.active)
            return false;
        directory.active = true;
        service->Register(directory);
        return true;
    }

    void StopSoftBodyWorldControl()
    {
        auto &directory = *Directory::Get();
        if (!directory.active)
            return;
        directory.active = false;
        auto *service    = Interface<phy::ISoftBodyWorldControl>::Get();
        if (service->GetApi() == &directory)
            service->UnRegister();
        directory.Drain();
    }
} // namespace sky::softbody

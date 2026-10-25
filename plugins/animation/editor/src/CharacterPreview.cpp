#include "animation/editor/CharacterPreview.h"

#include <algorithm>
#include <aurora/renderer/ISkeletalPoseSource.h>
#include <aurora/renderer/ImportedSkinBinding.h>
#include <cmath>
#include <editor/core/property/PropertyDescriptor.h>
#include <framework/application/ModuleManager.h>
#include <framework/asset/AssetDataBase.h>
#include <framework/asset/AssetManager.h>
#include <framework/interface/ISystem.h>
#include <framework/interface/Interface.h>
#include <framework/serialization/PropertyCommon.h>
#include <framework/world/ComponentFactory.h>
#include <framework/world/WorldSubSystemRegistry.h>
#include <map>

namespace sky::animation::editor {
    using namespace sky::editor;
    namespace {
        const TypeNode *ComponentType(std::string_view name)
        {
            const auto *node = SerializationContext::Get()->FindType(name);
            if (!node || !node->info)
                return nullptr;
            for (const auto &[group, types] : ComponentFactory::Get()->GetTypes())
                for (const auto &type : types)
                    if (type.typeId == node->info->registeredId)
                        return node;
            return nullptr;
        }
        bool Set(ComponentBase *component, const TypeNode &type, std::string_view name, const Any &value)
        {
            const auto field = type.members.find(name);
            return component && field != type.members.end() && field->second.info && value.Info() &&
                   field->second.info->registeredId == value.Info()->registeredId && field->second.setterFn &&
                   field->second.setterFn(component, value.Data());
        }
        bool SetEnum(ComponentBase *component, const TypeNode &type, std::string_view name, std::string_view entry)
        {
            const auto field = type.members.find(name);
            if (field == type.members.end() || !field->second.info || !field->second.info->staticInfo || !field->second.info->staticInfo->isEnum ||
                field->second.info->staticInfo->size > sizeof(uint64_t))
                return false;
            const auto *enumeration = GetTypeNode(field->second.info);
            if (!enumeration)
                return false;
            for (const auto &[value, label] : enumeration->enums)
                if (label == entry)
                    return Set(component, type, name, Any::Create(field->second.info, &value));
            return false;
        }
    } // namespace
    void RegisterCharacterPreviewSettings()
    {
        SerializationContext::Get()
            ->Register<CharacterPreviewSettings>("CharacterPreviewSettings")
            .Member<&CharacterPreviewSettings::mesh>("mesh")
            .Property(CommonPropertyKey::ASSET_TYPE, Any(std::string("AuroraMesh")))
            .Property(CommonPropertyKey::LABEL, Any(std::string("Mesh")))
            .Property(CommonPropertyKey::ORDER, Any(0))
            .Member<&CharacterPreviewSettings::skin>("skin")
            .Property(CommonPropertyKey::ASSET_TYPE, Any(std::string("AuroraSkin")))
            .Property(CommonPropertyKey::LABEL, Any(std::string("Skin")))
            .Property(CommonPropertyKey::ORDER, Any(1))
            .Member<&CharacterPreviewSettings::skeleton>("skeleton")
            .Property(CommonPropertyKey::ASSET_TYPE, Any(std::string("AuroraSkeleton")))
            .Property(CommonPropertyKey::LABEL, Any(std::string("Skeleton")))
            .Property(CommonPropertyKey::ORDER, Any(2))
            .Member<&CharacterPreviewSettings::material>("material")
            .Property(CommonPropertyKey::ASSET_TYPE, Any(std::string("AuroraMaterial")))
            .Property(CommonPropertyKey::LABEL, Any(std::string("Material override")))
            .Property(CommonPropertyKey::ORDER, Any(3))
            .Member<&CharacterPreviewSettings::referencePose>("referencePose")
            .Property(CommonPropertyKey::LABEL, Any(std::string("Reference pose")))
            .Property(CommonPropertyKey::ORDER, Any(4))
            .Member<&CharacterPreviewSettings::loop>("loop")
            .Property(CommonPropertyKey::ORDER, Any(5))
            .Member<&CharacterPreviewSettings::rate>("rate")
            .Property(CommonPropertyKey::ORDER, Any(6))
            .Member<&CharacterPreviewSettings::scrubSeconds>("scrubSeconds")
            .Property(CommonPropertyKey::LABEL, Any(std::string("Scrub time (seconds)")))
            .Property(CommonPropertyKey::ORDER, Any(7))
            .Member<&CharacterPreviewSettings::showBones>("showBones")
            .Property(CommonPropertyKey::LABEL, Any(std::string("Show bones")))
            .Property(CommonPropertyKey::ORDER, Any(8))
            .Member<&CharacterPreviewSettings::showBounds>("showBounds")
            .Property(CommonPropertyKey::LABEL, Any(std::string("Show bounds")))
            .Property(CommonPropertyKey::ORDER, Any(9));
    }
    std::shared_ptr<const void> RetainCharacterPreviewModules()
    {
        auto *system  = Interface<ISystemNotify>::Get()->GetApi();
        auto *manager = system ? system->GetModuleManager() : nullptr;
        if (!manager)
            return {};
        // The manager's lease retains declared dependencies and has a host-owned deleter.
        return manager->RetainModule("Animation.Editor");
    }
    AnimationStatus ApplyImportedCharacterDefaults(CharacterPreviewSettings &settings)
    {
        Uuid                        owner;
        std::map<std::string, Uuid> defaults;
        auto                       *db = AssetDataBase::Get();
        if (!settings.mesh || !db->GetOwner(settings.mesh, owner) || !db->GetGeneratedDefaults(owner, defaults))
            return {AnimationError::InvalidAsset, "Select an imported mesh with a generated binding manifest"};
        aurora::ImportedSkinBinding binding;
        if (aurora::ResolveImportedSkinBinding(settings.mesh, defaults, binding) != aurora::ImportedSkinBindingStatus::Ready)
            return {AnimationError::InvalidAsset, "Mesh has no unique imported binding; select Skin and Skeleton explicitly"};
        settings.skin     = binding.skin;
        settings.skeleton = binding.skeleton;
        return {};
    }
    CharacterPreview::CharacterPreview(Uuid asset, std::string type, std::shared_ptr<const void> code)
        : asset(asset), type(std::move(type)), code(std::move(code))
    {
        settings.referencePose = this->type == "AuroraSkeleton";
        if (settings.referencePose)
            settings.skeleton = asset;
    }
    CharacterPreview::~CharacterPreview()
    {
        Close();
    }
    void CharacterPreview::Close()
    {
        if (session.session) {
            if (auto *provider = AssetPreviewProviderRegistry::Get()->GetProvider()) {
                const auto closed = provider->CloseSession(session);
                if (const auto *error = std::get_if<AssetPreviewError>(&closed))
                    diagnostic = error->message;
            }
        }
        if (world)
            world->StopSimulation();
        world.Reset(nullptr);
        actor    = {};
        session  = {};
        output   = {};
        playback = {};
        controls.clear();
        parameters.clear();
    }
    void CharacterPreview::Configure(CharacterPreviewSettings value)
    {
        if (!std::isfinite(value.rate) || value.rate < 0 || !std::isfinite(value.scrubSeconds) || value.scrubSeconds < 0) {
            ReportError("Preview rate and scrub time must be finite and nonnegative");
            return;
        }
        inputDiagnostic.clear();
        rebuild = rebuild || value.mesh != settings.mesh || value.skin != settings.skin || value.skeleton != settings.skeleton ||
                  value.material != settings.material || value.referencePose != settings.referencePose || value.loop != settings.loop;
        const bool rateChanged  = value.rate != settings.rate;
        const bool scrubChanged = value.scrubSeconds != settings.scrubSeconds;
        settings                = std::move(value);
        if (!settings.referencePose && rateChanged)
            Control(AnimationControlOp::SetRate, settings.rate);
        if (!settings.referencePose && scrubChanged) {
            Control(AnimationControlOp::Pause);
            Control(AnimationControlOp::Seek, settings.scrubSeconds);
        }
    }
    void CharacterPreview::Resize(uint32_t w, uint32_t h)
    {
        width  = std::clamp(w, 1u, 4096u);
        height = std::clamp(h, 1u, 4096u);
    }
    void CharacterPreview::Control(AnimationControlOp op, float value)
    {
        if (op == AnimationControlOp::Reload) {
            requestedRig = {};
            rigRequest   = {};
            clipRequest  = {};
            graphAsset.reset();
            rebuild = rebuild || !world;
            inputDiagnostic.clear();
            if (settings.referencePose || !world)
                return;
        } else if (settings.referencePose) {
            ReportError("Reference pose preview has no playback controls");
            return;
        }
        if (controls.size() < 64) {
            inputDiagnostic.clear();
            controls.emplace_back(op, value);
        } else
            ReportError("Too many pending preview controls");
    }
    void CharacterPreview::SetParameter(const Uuid &id, AnimationParam value)
    {
        const auto found = std::ranges::find(parameters, id, &std::pair<Uuid, AnimationParam>::first);
        if (found != parameters.end())
            found->second = value;
        else if (parameters.size() < 256)
            parameters.emplace_back(id, std::move(value));
        else
            ReportError("Too many preview parameter overrides");
    }
    AnimationStatus CharacterPreview::FocusCharacter()
    {
        std::optional<AABB> bounds;
        const auto          include = [&](const AABB &box) {
            if (bounds)
                bounds->Merge(box);
            else
                bounds = box;
        };
        for (const auto &box : output.bounds)
            include(box);
        if (settings.mesh && !bounds)
            return {AnimationError::InvalidPlayback, "Waiting for mesh bounds before focusing the preview"};
        if (!bounds) {
            for (const auto &model : boneModels) {
                const auto p = model.m[3];
                include({{p.x, p.y, p.z}, {p.x, p.y, p.z}});
            }
        }
        if (!bounds)
            return {AnimationError::InvalidPlayback, "Load a skeleton or mesh before focusing the preview"};
        const auto  center   = (bounds->min + bounds->max) * .5f;
        const float halfFov  = std::atan(std::tan(camera.verticalFov * .5f) * std::min(1.f, float(width) / std::max(1u, height)));
        const float distance = (bounds->max - bounds->min).Length() * .55f / std::sin(halfFov);
        if (!std::isfinite(distance) || !std::isfinite(center.x) || !std::isfinite(center.y) || !std::isfinite(center.z))
            return {AnimationError::InvalidAsset, "Preview bounds are not finite"};
        camera.orbitDistance = std::max(1.f, distance);
        camera.Focus(center);
        return {};
    }
    void CharacterPreview::ResolveRig()
    {
        if (!settings.skeleton && type == "AnimationClip") {
            auto *source = Interface<IAnimationClipSource>::Get()->GetApi();
            if (!clipRequest && source)
                clipRequest = source->Request(asset);
            const auto clip = clipRequest.Poll();
            if (clip.state == AnimationSourceState::Ready && clip.skeletonHint) {
                settings.skeleton = clip.skeletonHint;
                rebuild           = true;
            } else if (clip.state == AnimationSourceState::Error)
                diagnostic = clip.status.message;
        }
        if (!settings.skeleton && IsGraph()) {
            const auto serial = AssetManager::Get()->GetProductChangeSerial();
            if (!graphAsset || graphSerial != serial) {
                graphAsset  = AssetManager::Get()->LoadAsset<GraphAsset>(asset, AssetLoadPolicy::ProductsOnly);
                graphSerial = serial;
            }
            if (graphAsset && graphAsset->IsLoaded() && graphAsset->Data().IsLoadValid()) {
                settings.skeleton = graphAsset->Data().graph.definition.skeleton;
                rebuild           = true;
            } else
                diagnostic = "Graph preview requires a successfully cooked product";
        }
        auto      *source = Interface<IAnimationSkeletonSource>::Get()->GetApi();
        const auto serial = source ? source->GetChangeSerial() : 0;
        if (!source) {
            requestedRig = {};
            rigRequest   = {};
            rig          = {};
            diagnostic   = "Skeleton provider is unavailable";
            return;
        }
        if (settings.skeleton != requestedRig || rigSerial != serial) {
            requestedRig = settings.skeleton;
            rigSerial    = serial;
            rigRequest   = source && requestedRig ? source->Request(requestedRig) : AnimationSourceRequest<Skeleton>{};
            rig          = {};
        }
        if (rigRequest) {
            rig = rigRequest.Poll();
            if (rig.state == AnimationSourceState::Error)
                diagnostic = rig.status.message;
        }
    }
    bool CharacterPreview::BuildWorld()
    {
        auto pendingControls   = std::move(controls);
        auto pendingParameters = std::move(parameters);
        Close();
        controls     = std::move(pendingControls);
        parameters   = std::move(pendingParameters);
        rebuild      = false;
        frameOnReady = true;
        diagnostic.clear();
        auto *provider = AssetPreviewProviderRegistry::Get()->GetProvider();
        if (!provider || !code) {
            diagnostic = "Preview renderer or component module lease is unavailable";
            return false;
        }
        if (!settings.skeleton) {
            diagnostic = "Select a Skeleton for this preview";
            return false;
        }
        if (settings.mesh && !settings.skin) {
            diagnostic = "A mesh preview requires an explicit Skin";
            return false;
        }
        auto next = WorldPtr(World::CreateWorld());
        if (!settings.referencePose) {
            const auto *registration = WorldSubSystemRegistry::Get().GetRegistration(Name("Animation"));
            if (!registration || !Interface<IAnimationInstanceControl>::Get()->GetApi()) {
                diagnostic = "Animation.World provider unavailable";
                return false;
            }
            if (registration->validate && !registration->validate({}, diagnostic))
                return false;
            auto system = registration->factory(*next, {});
            if (!system) {
                diagnostic = "Cannot create preview Animation subsystem";
                return false;
            }
            next->AddSubSystem(Name("Animation"), system.release(), registration->moduleLease);
        }
        auto character = std::make_unique<Actor>(Uuid::Create());
        character->SetName("Preview character");
        if (settings.mesh) {
            const auto *typeNode  = ComponentType("SkeletalMeshComponent");
            auto       *component = typeNode ? character->AddComponent(typeNode->info->registeredId) : nullptr;
            if (!component || !Set(component, *typeNode, "mesh", Any(settings.mesh)) || !Set(component, *typeNode, "skin", Any(settings.skin)) ||
                !Set(component, *typeNode, "skeleton", Any(settings.skeleton)) || !Set(component, *typeNode, "material", Any(settings.material)) ||
                !SetEnum(component, *typeNode, "pose", settings.referencePose ? "ReferencePose" : "SameActorAnimation")) {
                diagnostic = "Registered SkeletalMesh component does not provide the preview property contract";
                return false;
            }
        }
        if (!settings.referencePose) {
            const auto *typeNode  = ComponentType("AnimationComponent");
            auto       *component = typeNode ? character->AddComponent(typeNode->info->registeredId) : nullptr;
            if (!component || !Set(component, *typeNode, "skeleton", Any(settings.skeleton)) ||
                !Set(component, *typeNode, IsGraph() ? "graph" : "clip", Any(asset)) || !Set(component, *typeNode, "autoplay", Any(false)) ||
                !Set(component, *typeNode, "loop", Any(settings.loop)) || !Set(component, *typeNode, "playRate", Any(settings.rate)) ||
                !SetEnum(component, *typeNode, "mode", IsGraph() ? "Graph" : "Clip")) {
                diagnostic = "Registered Animation component does not provide the preview property contract";
                return false;
            }
        }
        actor = character->GetUuid();
        if (!next->AttachToWorld(std::move(character))) {
            diagnostic = "Cannot attach preview character";
            return false;
        }
        auto created = provider->CreateSession({code, next, camera, width, height});
        if (const auto *error = std::get_if<AssetPreviewError>(&created)) {
            diagnostic = error->message;
            return false;
        }
        session = std::get<AssetPreviewHandle>(created);
        world   = std::move(next);
        world->StartSimulation();
        return true;
    }
    void CharacterPreview::Tick(float delta)
    {
        if (world)
            diagnostic.clear();
        ResolveRig();
        if (rebuild)
            BuildWorld();
        if (!world)
            return;
        auto *control = Interface<IAnimationInstanceControl>::Get()->GetApi();
        if (!settings.referencePose && !control)
            diagnostic = "Animation.World provider was unloaded";
        if (!settings.referencePose && control) {
            playback = control->Find(world.Get(), actor);
            if (playback.state == AnimationSourceState::Ready) {
                for (const auto &[op, value] : controls) {
                    const auto status = control->Control(playback.handle, op, value);
                    if (!status)
                        diagnostic = status.message;
                }
                controls.clear();
                for (const auto &[id, value] : parameters) {
                    const auto status = control->SetParameter(playback.handle, id, value);
                    if (!status)
                        diagnostic = status.message;
                }
                parameters.clear();
            }
        }
        world->Tick(std::isfinite(delta) && delta >= 0 ? delta : 0);
        if (!settings.referencePose && control) {
            playback = control->Find(world.Get(), actor);
            if (!playback.diagnostic)
                diagnostic = playback.diagnostic.message;
        }
        boneModels.clear();
        if (settings.referencePose && rig.state == AnimationSourceState::Ready && rig.resource) {
            const auto status = rig.resource->GetRefPos()->ToSkinRenderData(boneModels, Transform{});
            if (!status)
                diagnostic = status.message;
        } else if (auto *poses = Interface<aurora::ISkeletalPoseSource>::Get()->GetApi()) {
            auto pose = poses->Describe(world.Get(), actor);
            if (pose.state == aurora::SkeletalPoseState::Ready && rig.state == AnimationSourceState::Ready && rig.version == playback.skeleton)
                boneModels = std::move(pose.models);
        }
        auto *provider = AssetPreviewProviderRegistry::Get()->GetProvider();
        if (!provider) {
            diagnostic = "Preview renderer was closed";
            output     = {};
            return;
        }
        if (frameOnReady && FocusCharacter())
            frameOnReady = false;
        const auto resized = provider->Resize(session, width, height);
        const auto updated = provider->Update(session, camera);
        const auto state   = provider->Status(session);
        if (const auto *error = std::get_if<AssetPreviewError>(&resized))
            diagnostic = error->message;
        if (const auto *error = std::get_if<AssetPreviewError>(&updated))
            diagnostic = error->message;
        if (const auto *error = std::get_if<AssetPreviewError>(&state)) {
            diagnostic = error->message;
            output     = {};
            if (error->code == AssetPreviewErrorCode::StaleSession)
                rebuild = true;
        } else {
            output = std::get<AssetPreviewStatus>(state);
            if (!output.diagnostic.empty())
                diagnostic = output.diagnostic;
        }
    }
} // namespace sky::animation::editor

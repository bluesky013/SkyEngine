#include "WorldEditorState.h"

#include <animation/IAnimationInstanceControl.h>
#include <aurora/adaptor/components/CameraComponent.h>
#include <aurora/adaptor/components/DirectLightComponent.h>
#include <aurora/adaptor/components/EnvironmentComponent.h>
#include <aurora/adaptor/components/PointLightComponent.h>
#include <aurora/adaptor/components/SkeletalMeshComponent.h>
#include <aurora/adaptor/components/StaticMeshComponent.h>
#include <aurora/adaptor/renderer/SceneAssetSource.h>
#include <aurora/renderer/ImportedSkinBinding.h>
#include <core/logger/Logger.h>
#include <editor/core/EditorCore.h>
#include <editor/core/actor/EditorActorCreation.h>
#include <framework/asset/AssetDataBase.h>
#include <framework/interface/Interface.h>
#include <framework/world/ComponentFactory.h>
#include <framework/world/TransformComponent.h>
#include <framework/world/WorldSubSystemRegistry.h>
#include <optional>
#include <unordered_map>
#include <unordered_set>

namespace sky::editor {
    namespace {
        bool RegisteredComponent(const Uuid &id)
        {
            return ComponentFactory::Get()->Find(id) != nullptr;
        }

        bool CollectDependencies(WorldDocument &document, const std::vector<Uuid> &types, std::vector<UndoCommandPtr> &commands)
        {
            std::unordered_set<Name> visited;
            for (const auto &type : types) {
                const auto *info = ComponentFactory::Get()->Find(type);
                if (!info)
                    return false;
                for (const auto &name : info->requiredSubSystems) {
                    if (!visited.insert(name).second)
                        continue;
                    if (!WorldSubSystemRegistry::Get().IsRegistered(name)) {
                        LOG_E("WorldEditor", "Required subsystem is unavailable: %s", name.GetStr().data());
                        return false;
                    }
                    bool enabled = false;
                    if (!document.IsSubSystemEnabled(std::string(name.GetStr()), enabled) || !enabled) {
                        commands.push_back(document.MakeSubSystemEnabledCommand(std::string(name.GetStr()), true));
                        if (!commands.back())
                            return false;
                    }
                }
            }
            return true;
        }

        class CreationCommand final : public UndoCommand {
        public:
            CreationCommand(UndoCommandPtr edit, std::vector<UndoCommandPtr> dependencies, std::shared_ptr<const ActorCreation> creator = {})
                : UndoCommand(edit->GetName()), creator(std::move(creator)), dependencies(std::move(dependencies)), edit(std::move(edit))
            {
            }
            void Do() override
            {
                TryDo();
            }
            void Undo() override
            {
                TryUndo();
            }
            bool TryDo() override
            {
                size_t applied = 0;
                for (; applied < dependencies.size(); ++applied) {
                    if (!dependencies[applied]->TryDo())
                        break;
                }
                if (applied == dependencies.size() && edit->TryDo())
                    return true;
                while (applied > 0)
                    dependencies[--applied]->Undo();
                return false;
            }
            bool TryUndo() override
            {
                if (!edit->TryUndo())
                    return false;
                for (auto it = dependencies.rbegin(); it != dependencies.rend(); ++it)
                    (*it)->Undo();
                return true;
            }

        private:
            std::shared_ptr<const ActorCreation> creator;
            std::vector<UndoCommandPtr>          dependencies;
            UndoCommandPtr                       edit;
        };

        bool SetComponentSkeleton(ComponentBase &component, const Uuid &skeleton)
        {
            const auto *node = GetTypeNode(component.GetTypeId());
            if (!node || !component.GetReflectedObject())
                return false;
            const auto member = node->members.find("skeleton");
            return member != node->members.end() &&
                   PropertyDescriptor(component.GetReflectedObject(), &member->second, "skeleton", "").SetValue(Any(skeleton));
        }

        class AddComponentCommand final : public UndoCommand {
        public:
            AddComponentCommand(Actor &actor, Actor::ComponentPtr component, aurora::SkeletalMeshComponent *skeletal, std::function<void()> changed)
                : UndoCommand("Add component")
                , actor(actor)
                , type(component->GetTypeId())
                , detached(std::move(component))
                , skeletal(skeletal)
                , changed(std::move(changed))
            {
                if (skeletal)
                    previousPose = skeletal->GetPoseMode();
            }
            void Do() override
            {
                TryDo();
            }
            void Undo() override
            {
                TryUndo();
            }
            bool TryDo() override
            {
                if (!detached || !actor.AttachComponent(detached))
                    return false;
                if (skeletal)
                    skeletal->SetPoseMode(aurora::SkeletalPoseMode::SameActorAnimation);
                changed();
                return true;
            }
            bool TryUndo() override
            {
                detached = actor.DetachComponent(type);
                if (!detached)
                    return false;
                if (skeletal)
                    skeletal->SetPoseMode(previousPose);
                changed();
                return true;
            }

        private:
            Actor                         &actor;
            Uuid                           type;
            Actor::ComponentPtr            detached;
            aurora::SkeletalMeshComponent *skeletal;
            aurora::SkeletalPoseMode       previousPose = aurora::SkeletalPoseMode::ReferencePose;
            std::function<void()>          changed;
        };

        class SkeletalBindingCommand final : public UndoCommand {
        public:
            SkeletalBindingCommand(aurora::SkeletalMeshComponent &component, aurora::SkeletalMeshComponentData after)
                : UndoCommand("Select skeletal mesh"), component(component), before(component.GetData()), after(std::move(after))
            {
            }
            void Do() override
            {
                component.SetConfig(after);
            }
            void Undo() override
            {
                component.SetConfig(before);
            }

        private:
            aurora::SkeletalMeshComponent    &component;
            aurora::SkeletalMeshComponentData before;
            aurora::SkeletalMeshComponentData after;
        };

        class WorldPropertyCommand final : public UndoCommand {
        public:
            WorldPropertyCommand(UndoCommandPtr edit, WorldDocument &document)
                : UndoCommand(edit->GetName()), edit(std::move(edit)), document(document)
            {
            }
            void Do() override
            {
                TryDo();
            }
            void Undo() override
            {
                TryUndo();
            }
            bool TryDo() override
            {
                if (!edit->TryDo())
                    return false;
                document.MarkDirty();
                return true;
            }
            bool TryUndo() override
            {
                if (!edit->TryUndo())
                    return false;
                document.MarkDirty();
                return true;
            }

        private:
            UndoCommandPtr edit;
            WorldDocument &document;
        };

        class ActorCommand final : public UndoCommand {
        public:
            ActorCommand(WorldPtr world, std::unique_ptr<Actor> actor, bool create, std::function<void()> changed)
                : UndoCommand(create ? "Create scene object" : "Delete scene object")
                , world(std::move(world))
                , detached(std::move(actor))
                , id(detached->GetUuid())
                , create(create)
                , changed(std::move(changed))
            {
            }
            void Do() override
            {
                TryDo();
            }
            void Undo() override
            {
                TryUndo();
            }
            bool TryDo() override
            {
                return Apply(create);
            }
            bool TryUndo() override
            {
                return Apply(!create);
            }

        private:
            bool Apply(bool attach)
            {
                if (attach) {
                    if (!world->TryAttachToWorld(detached))
                        return false;
                } else if (auto *actor = world->GetActorByUuid(id)) {
                    detached = world->DetachFromWorld(actor);
                } else if (!detached) {
                    return false;
                }
                changed();
                return true;
            }
            WorldPtr               world;
            std::unique_ptr<Actor> detached;
            Uuid                   id;
            bool                   create;
            std::function<void()>  changed;
        };
    } // namespace
    WorldEditorState::~WorldEditorState()
    {
        Bind(nullptr);
    }
    void WorldEditorState::Bind(WorldDocument *next)
    {
        if (next == document && (!next || next->GetWorld() == world.Get()))
            return;
        selection.Clear();
        pendingComponents.clear();
        refreshSelection = false;
        EditorCore::GetCommandService().Clear();
        if (world)
            WorldEvent::DisConnect(world.Get(), this);
        document = next;
        world    = next ? next->GetWorld() : nullptr;
        if (world)
            WorldEvent::Connect(world.Get(), this);
    }
    void WorldEditorState::OnActorDetached(Actor *)
    {
        // Selection is invalidated before the actor's reflected storage can be released.
        selection.Clear();
    }
    std::vector<PropertyObject> WorldEditorState::Resolve(const Uuid &id) const
    {
        if (!world)
            return {};
        auto *actor = world->GetActorByUuid(id);
        if (!actor) {
            return {};
        }
        std::vector<PropertyObject> result;
        const auto                  append = [&](ComponentBase *component) {
            if (component && component->GetReflectedObject()) {
                if (const auto *type = GetTypeNode(component->GetTypeId())) {
                    result.push_back({component->GetReflectedObject(), type});
                }
            }
        };
        append(actor->GetComponent<TransformComponent>());
        for (const auto &[type, component] : actor->GetComponents()) {
            if (type != TypeInfo<TransformComponent>::RegisteredId()) {
                append(component.get());
            }
        }
        return result;
    }
    std::vector<EditorOutlineItem> WorldEditorState::Outline() const
    {
        std::vector<EditorOutlineItem> result;
        if (!world)
            return result;
        std::unordered_map<const Actor *, std::vector<Actor *>> children;
        for (const auto &actor : world->GetActors()) {
            const auto *transform       = actor->GetComponent<TransformComponent>();
            const auto *parentTransform = transform ? transform->GetParent() : nullptr;
            const auto *parent          = parentTransform ? parentTransform->GetActor() : nullptr;
            if (parent && parent->GetWorld() != world.Get()) {
                parent = nullptr;
            }
            children[parent].push_back(actor.get());
        }
        std::vector<std::pair<Actor *, uint32_t>> pending;
        const auto                                push = [&](const Actor *parent, uint32_t depth) {
            const auto found = children.find(parent);
            if (found != children.end()) {
                for (auto it = found->second.rbegin(); it != found->second.rend(); ++it) {
                    pending.emplace_back(*it, depth);
                }
            }
        };
        push(nullptr, 0);
        while (!pending.empty()) {
            const auto [actor, depth] = pending.back();
            pending.pop_back();
            result.push_back({actor->GetUuid(), actor->GetName(), depth});
            push(actor, depth + 1);
        }
        return result;
    }
    void WorldEditorState::OnEdited(const Uuid &)
    {
        if (document)
            document->MarkDirty();
    }
    UndoCommandPtr WorldEditorState::MakeEditCommand(const Uuid &owner, const PropertyDescriptor &property, Any value)
    {
        if (!document || !world)
            return {};
        auto       *actor     = world->GetActorByUuid(owner);
        auto       *component = actor ? actor->GetComponent<aurora::SkeletalMeshComponent>() : nullptr;
        const auto *type      = component ? GetTypeNode(component->GetTypeId()) : nullptr;
        const auto *mesh      = value.GetAsConst<Uuid>();
        if (component && type && mesh && property.IsBoundTo(component->GetReflectedObject()) && property.CanEdit() &&
            type->members.contains("mesh") && property.GetMember() == &type->members.at("mesh")) {
            auto after = component->GetData();
            after.mesh = *mesh;
            if (!after.skin && !after.skeleton) {
                Uuid                        source;
                std::map<std::string, Uuid> defaults;
                aurora::ImportedSkinBinding binding;
                auto                       *database = AssetDataBase::Get();
                if (database->GetOwner(*mesh, source) && database->GetGeneratedDefaults(source, defaults) &&
                    aurora::ResolveImportedSkinBinding(*mesh, defaults, binding) == aurora::ImportedSkinBindingStatus::Ready) {
                    after.skin     = binding.skin;
                    after.skeleton = binding.skeleton;
                }
            }
            return std::make_unique<WorldPropertyCommand>(std::make_unique<SkeletalBindingCommand>(*component, std::move(after)), *document);
        }
        auto edit = property.MakeEditCommand(std::move(value));
        if (!document || !edit)
            return {};
        // Bind clears history before releasing its document and reflected storage.
        return std::make_unique<WorldPropertyCommand>(std::move(edit), *document);
    }
    std::vector<std::string> RegisterBuiltinActorCreators(std::shared_ptr<const void> lease)
    {
        std::vector<std::string> ids;
        const auto               add = [&](const char *id, const char *label, std::function<bool(Actor &)> create) {
            if (EditorActorCreation::Get()->RegisterCreation({lease, id, label, "Scene", static_cast<int>(ids.size()), std::move(create)}))
                ids.emplace_back(id);
        };
        using namespace aurora;
        add("empty", "Empty Object", [](Actor &actor) {
            actor.SetName("Actor");
            return true;
        });
        for (bool cube : {true, false}) {
            add(cube ? "cube" : "plane", cube ? "Cube" : "Plane", [cube](Actor &actor) {
                actor.SetName(cube ? "Cube" : "Plane");
                auto *mesh = actor.AddComponent<StaticMeshComponent>();
                mesh->SetMeshUuid(ScenePrimitiveId(cube ? ScenePrimitive::CUBE : ScenePrimitive::PLANE));
                mesh->SetMaterialUuid(SceneLitMaterialId());
                mesh->SetCastShadow(true);
                mesh->SetReceiveShadow(true);
                if (!cube)
                    actor.GetComponent<TransformComponent>()->SetLocalTranslation({0, -.5f, 0});
                return true;
            });
        }
        add("point-light", "Point Light", [](Actor &actor) {
            actor.SetName("Point Light");
            actor.AddComponent<PointLightComponent>();
            actor.GetComponent<TransformComponent>()->SetLocalTranslation({2, 2, 2});
            return true;
        });
        add("directional-light", "Directional Light", [](Actor &actor) {
            actor.SetName("Directional Light");
            actor.AddComponent<DirectLightComponent>()->SetCastShadow(true);
            actor.GetComponent<TransformComponent>()->SetLocalRotationEuler({-40, 30, 0});
            return true;
        });
        add("camera", "Camera", [](Actor &actor) {
            actor.SetName("Camera");
            actor.AddComponent<CameraComponent>();
            actor.GetComponent<TransformComponent>()->SetLocalTranslation({0, 0, 5});
            return true;
        });
        add("environment", "Environment", [](Actor &actor) {
            actor.SetName("Environment");
            actor.AddComponent<EnvironmentComponent>();
            return true;
        });
        return ids;
    }

    bool WorldEditorState::Create(const std::string &creatorId)
    {
        const auto creator = EditorActorCreation::Get()->Find(creatorId);
        if (!world || !document || !creator)
            return false;
        auto actor = std::make_unique<Actor>(Uuid::Create());
        actor->AddComponent<TransformComponent>();
        if (!creator->create(*actor))
            return false;
        std::vector<Uuid> types;
        for (const auto &[type, component] : actor->GetComponents())
            types.push_back(type);
        std::vector<UndoCommandPtr> dependencies;
        if (!CollectDependencies(*document, types, dependencies))
            return false;
        const auto id   = actor->GetUuid();
        auto       edit = std::make_unique<ActorCommand>(world, std::move(actor), true, [this]() { OnEdited({}); });
        if (!EditorCore::GetCommandService().Execute(std::make_unique<CreationCommand>(std::move(edit), std::move(dependencies), creator)))
            return false;
        selection.SetSelection({{SelectionType::ENTITY, id}});
        return true;
    }
    void WorldEditorState::DeleteSelection()
    {
        if (!world || selection.IsEmpty() || selection.GetSelection().front().type != SelectionType::ENTITY)
            return;
        const auto id    = selection.GetSelection().front().id;
        auto      *actor = world->GetActorByUuid(id);
        if (!actor)
            return;
        auto detached = world->DetachFromWorld(actor);
        EditorCore::GetCommandService().Execute(std::make_unique<ActorCommand>(world, std::move(detached), false, [this]() { OnEdited({}); }));
    }

    bool WorldEditorState::CanAddComponent(const Uuid &type) const
    {
        if (!world || !document || selection.GetSelection().size() != 1 || selection.GetSelection().front().type != SelectionType::ENTITY ||
            !RegisteredComponent(type))
            return false;
        const auto *info = ComponentFactory::Get()->Find(type);
        for (const auto &required : info->requiredSubSystems)
            if (!WorldSubSystemRegistry::Get().IsRegistered(required))
                return false;
        const auto id    = selection.GetSelection().front().id;
        auto      *actor = world->GetActorByUuid(id);
        return actor && !actor->GetComponent(type);
    }

    bool WorldEditorState::RequestAddComponent(const Uuid &type)
    {
        if (!CanAddComponent(type) || pendingComponents.size() >= 16)
            return false;
        pendingComponents.emplace_back(selection.GetSelection().front().id, type);
        return true;
    }

    void WorldEditorState::Tick()
    {
        if (document && document->GetWorld() != world.Get())
            Bind(document);
        const auto requests = std::move(pendingComponents);
        pendingComponents.clear();
        for (const auto &[id, type] : requests) {
            auto *actor = world ? world->GetActorByUuid(id) : nullptr;
            if (!actor || actor->GetComponent(type) || !RegisteredComponent(type))
                continue;
            std::vector<UndoCommandPtr> dependencies;
            if (!CollectDependencies(*document, {type}, dependencies))
                continue;
            auto component = ComponentFactory::Get()->Create(type);
            if (!component) {
                LOG_E("WorldEditor", "Registered component could not be constructed");
                continue;
            }
            auto      *control   = Interface<IAnimationInstanceControl>::Get()->GetApi();
            const bool animation = control && control->GetComponentType() == type;
            auto      *skeletal  = animation ? actor->GetComponent<aurora::SkeletalMeshComponent>() : nullptr;
            if (skeletal) {
                if (!SetComponentSkeleton(*component, skeletal->GetSkeletonUuid())) {
                    LOG_E("WorldEditor", "Animation authoring component lacks a writable Skeleton UUID");
                    continue;
                }
            }
            auto edit = std::make_unique<AddComponentCommand>(*actor, std::move(component), skeletal, [this]() {
                OnEdited({});
                refreshSelection = true;
            });
            EditorCore::GetCommandService().Execute(std::make_unique<CreationCommand>(std::move(edit), std::move(dependencies)));
        }
        if (refreshSelection) {
            refreshSelection    = false;
            const auto selected = selection.GetSelection();
            selection.Clear();
            selection.SetSelection(selected);
        }
    }
} // namespace sky::editor

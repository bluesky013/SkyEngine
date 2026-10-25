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
#include <framework/asset/AssetDataBase.h>
#include <framework/interface/Interface.h>
#include <framework/world/ComponentFactory.h>
#include <framework/world/TransformComponent.h>
#include <framework/world/WorldSubSystemRegistry.h>
#include <optional>
#include <unordered_map>

namespace sky::editor {
    namespace {
        bool RegisteredComponent(const Uuid &id)
        {
            for (const auto &[group, types] : ComponentFactory::Get()->GetTypes())
                for (const auto &type : types)
                    if (type.typeId == id)
                        return true;
            return false;
        }

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
            AddComponentCommand(Actor                         &actor,
                                Uuid                           type,
                                aurora::SkeletalMeshComponent *skeletal,
                                WorldDocument                 &document,
                                std::string                    system,
                                std::function<void()>          changed)
                : UndoCommand("Add component")
                , actor(actor)
                , type(type)
                , skeletal(skeletal)
                , document(document)
                , system(std::move(system))
                , changed(std::move(changed))
            {
                if (skeletal)
                    previousPose = skeletal->GetPoseMode();
                bool enabled = false;
                if (!this->system.empty() && document.IsSubSystemEnabled(this->system, enabled))
                    previousSystemEnabled = enabled;
            }
            void Do() override
            {
                if (detached && !actor.AttachComponent(detached)) {
                    LOG_E("WorldEditor", "Cannot restore component: actor already has this component type");
                    return;
                }
                if (skeletal)
                    skeletal->SetPoseMode(aurora::SkeletalPoseMode::SameActorAnimation);
                if (!system.empty())
                    document.SetSubSystemEnabled(system, true);
                changed();
            }
            void Undo() override
            {
                detached = actor.DetachComponent(type);
                if (skeletal)
                    skeletal->SetPoseMode(previousPose);
                if (!system.empty()) {
                    if (previousSystemEnabled)
                        document.SetSubSystemEnabled(system, *previousSystemEnabled);
                    else
                        std::erase_if(document.GetWorld()->GetMutableWorldDesc()->subSystems,
                                      [this](const auto &entry) { return entry.name == system.c_str(); });
                }
                changed();
            }

        private:
            Actor                         &actor;
            Uuid                           type;
            Actor::ComponentPtr            detached;
            aurora::SkeletalMeshComponent *skeletal;
            aurora::SkeletalPoseMode       previousPose = aurora::SkeletalPoseMode::ReferencePose;
            WorldDocument                 &document;
            std::string                    system;
            std::optional<bool>            previousSystemEnabled;
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
                edit->Do();
                document.MarkDirty();
            }
            void Undo() override
            {
                edit->Undo();
                document.MarkDirty();
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
                Apply(create);
            }
            void Undo() override
            {
                Apply(!create);
            }

        private:
            void Apply(bool attach)
            {
                if (attach) {
                    if (detached)
                        world->AttachToWorld(std::move(detached));
                } else if (auto *actor = world->GetActorByUuid(id))
                    detached = world->DetachFromWorld(actor);
                changed();
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
        if (next == document)
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
    void WorldEditorState::Create(EditorSceneObject kind)
    {
        if (!world || !document)
            return;
        auto  actor     = std::make_unique<Actor>(Uuid::Create());
        auto *transform = actor->AddComponent<TransformComponent>();
        using namespace aurora;
        switch (kind) {
        case EditorSceneObject::EMPTY: actor->SetName("Actor"); break;
        case EditorSceneObject::CUBE:
        case EditorSceneObject::PLANE: {
            const bool cube = kind == EditorSceneObject::CUBE;
            actor->SetName(cube ? "Cube" : "Plane");
            auto *mesh = actor->AddComponent<StaticMeshComponent>();
            mesh->SetMeshUuid(ScenePrimitiveId(cube ? ScenePrimitive::CUBE : ScenePrimitive::PLANE));
            mesh->SetMaterialUuid(SceneLitMaterialId());
            mesh->SetCastShadow(true);
            mesh->SetReceiveShadow(true);
            if (!cube)
                transform->SetLocalTranslation({0, -.5f, 0});
            break;
        }
        case EditorSceneObject::POINT_LIGHT:
            actor->SetName("Point Light");
            actor->AddComponent<PointLightComponent>();
            transform->SetLocalTranslation({2, 2, 2});
            break;
        case EditorSceneObject::DIRECTIONAL_LIGHT:
            actor->SetName("Directional Light");
            actor->AddComponent<DirectLightComponent>()->SetCastShadow(true);
            transform->SetLocalRotationEuler({-40, 30, 0});
            break;
        case EditorSceneObject::CAMERA:
            actor->SetName("Camera");
            actor->AddComponent<CameraComponent>();
            transform->SetLocalTranslation({0, 0, 5});
            break;
        case EditorSceneObject::ENVIRONMENT:
            actor->SetName("Environment");
            actor->AddComponent<EnvironmentComponent>();
            break;
        }
        const auto id = actor->GetUuid();
        EditorCore::GetCommandService().Execute(std::make_unique<ActorCommand>(world, std::move(actor), true, [this]() { OnEdited({}); }));
        selection.SetSelection({{SelectionType::ENTITY, id}});
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
        const auto requests = std::move(pendingComponents);
        pendingComponents.clear();
        for (const auto &[id, type] : requests) {
            auto *actor = world ? world->GetActorByUuid(id) : nullptr;
            if (!actor || actor->GetComponent(type) || !RegisteredComponent(type))
                continue;
            auto *component = actor->AddComponent(type);
            if (!component) {
                LOG_E("WorldEditor", "Registered component could not be constructed");
                continue;
            }
            auto             *control   = Interface<IAnimationInstanceControl>::Get()->GetApi();
            const bool        animation = control && control->GetComponentType() == type;
            const std::string system    = animation ? std::string(control->GetWorldSystemName()) : std::string{};
            if (!system.empty() && !WorldSubSystemRegistry::Get().IsRegistered(Name(system.c_str()))) {
                actor->RemoveComponent(type);
                LOG_E("WorldEditor", "Animation authoring subsystem is unavailable");
                continue;
            }
            auto *skeletal = animation ? actor->GetComponent<aurora::SkeletalMeshComponent>() : nullptr;
            if (skeletal) {
                if (!SetComponentSkeleton(*component, skeletal->GetSkeletonUuid())) {
                    actor->RemoveComponent(type);
                    LOG_E("WorldEditor", "Animation authoring component lacks a writable Skeleton UUID");
                    continue;
                }
            }
            EditorCore::GetCommandService().Execute(std::make_unique<AddComponentCommand>(*actor, type, skeletal, *document, system, [this]() {
                OnEdited({});
                refreshSelection = true;
            }));
        }
        if (refreshSelection) {
            refreshSelection    = false;
            const auto selected = selection.GetSelection();
            selection.Clear();
            selection.SetSelection(selected);
        }
    }
} // namespace sky::editor

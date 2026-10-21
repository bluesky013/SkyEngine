#include "WorldEditorState.h"

#include <aurora/adaptor/components/CameraComponent.h>
#include <aurora/adaptor/components/DirectLightComponent.h>
#include <aurora/adaptor/components/EnvironmentComponent.h>
#include <aurora/adaptor/components/PointLightComponent.h>
#include <aurora/adaptor/components/StaticMeshComponent.h>
#include <aurora/adaptor/renderer/SceneAssetSource.h>
#include <editor/core/EditorCore.h>
#include <framework/world/TransformComponent.h>
#include <unordered_map>

namespace sky::editor {
    namespace {
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
    UndoCommandPtr WorldEditorState::MakeEditCommand(const Uuid &, const PropertyDescriptor &property, Any value)
    {
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
} // namespace sky::editor

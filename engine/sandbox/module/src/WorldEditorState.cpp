#include "WorldEditorState.h"

#include <core/logger/Logger.h>
#include <editor/core/EditorCore.h>
#include <editor/core/actor/EditorActorCreation.h>
#include <editor/core/extension/EditorExtensionHost.h>
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

        struct EditContext {
            WorldDocument      *document = nullptr;
            std::weak_ptr<void> documentLifetime, bindingLifetime;
            WorldPtr            world;
            Uuid                runtime, persistent;
            EditContext(WorldDocument &document, std::weak_ptr<void> binding)
                : document(&document)
                , documentLifetime(document.GetEditLifetime())
                , bindingLifetime(std::move(binding))
                , world(document.GetWorld())
                , runtime(world->GetRuntimeId())
                , persistent(world->GetPersistID())
            {
            }
            bool Valid() const
            {
                return !documentLifetime.expired() && !bindingLifetime.expired() && document->GetWorld() == world.Get() &&
                       world->GetRuntimeId() == runtime && world->GetPersistID() == persistent;
            }
        };

        class CreationCommand final : public UndoCommand {
        public:
            CreationCommand(EditContext                          context,
                            UndoCommandPtr                       edit,
                            std::vector<UndoCommandPtr>          dependencies,
                            std::shared_ptr<const ActorCreation> creator = {})
                : UndoCommand(edit->GetName()), context(std::move(context)), creator(std::move(creator)), steps(std::move(dependencies))
            {
                steps.push_back(std::move(edit));
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
                if (!context.Valid())
                    return false;
                const bool dirty = context.document->IsDirty();
                for (size_t applied = 0; applied < steps.size(); ++applied) {
                    if (!context.Valid())
                        return false;
                    if (steps[applied]->TryDo())
                        continue;
                    bool restored = true;
                    while (applied > 0)
                        restored = steps[--applied]->TryUndo() && restored;
                    if (!restored)
                        LOG_E("WorldEditor", "Creation rollback failed");
                    else if (!dirty && context.Valid())
                        context.document->ClearDirty();
                    return false;
                }
                return true;
            }
            bool TryUndo() override
            {
                if (!context.Valid())
                    return false;
                const bool dirty = context.document->IsDirty();
                for (size_t remaining = steps.size(); remaining > 0; --remaining) {
                    if (!context.Valid())
                        return false;
                    if (steps[remaining - 1]->TryUndo())
                        continue;
                    bool restored = true;
                    for (size_t redo = remaining; redo < steps.size(); ++redo)
                        restored = steps[redo]->TryDo() && restored;
                    if (!restored)
                        LOG_E("WorldEditor", "Creation undo compensation failed");
                    else if (!dirty && context.Valid())
                        context.document->ClearDirty();
                    return false;
                }
                return true;
            }

        private:
            EditContext                          context;
            std::shared_ptr<const ActorCreation> creator;
            std::vector<UndoCommandPtr>          steps;
        };

        class AddComponentCommand final : public UndoCommand {
        public:
            AddComponentCommand(
                EditContext context, const Actor &owner, Actor::ComponentPtr component, UndoCommandPtr related, std::function<void()> changed)
                : UndoCommand("Add component")
                , context(std::move(context))
                , actor(owner.GetUuid())
                , actorIncarnation(owner.GetIncarnation())
                , type(component->GetTypeId())
                , expected(component->GetIncarnation())
                , detached(std::move(component))
                , related(std::move(related))
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
                if (!context.Valid())
                    return false;
                auto *target = context.world->GetActorByUuid(actor);
                if (!target || target->GetIncarnation() != actorIncarnation)
                    return false;
                if (!detached || detached->GetIncarnation() != expected || !target->AttachComponent(detached))
                    return false;
                if (related && !related->TryDo()) {
                    detached = target->DetachComponent(type);
                    if (!detached)
                        LOG_E("WorldEditor", "Component edit rollback could not detach its component");
                    return false;
                }
                if (!context.Valid())
                    return false;
                changed();
                return true;
            }
            bool TryUndo() override
            {
                if (!context.Valid())
                    return false;
                auto *target = context.world->GetActorByUuid(actor);
                if (!target || target->GetIncarnation() != actorIncarnation)
                    return false;
                if (!target->GetComponent(type) || target->GetComponent(type)->GetIncarnation() != expected || (related && !related->TryUndo()))
                    return false;
                detached = target->DetachComponent(type);
                if (!detached) {
                    if (related && !related->TryDo())
                        LOG_E("WorldEditor", "Component undo compensation failed");
                    return false;
                }
                if (!context.Valid())
                    return false;
                changed();
                return true;
            }

        private:
            EditContext           context;
            Uuid                  actor, actorIncarnation, type, expected;
            Actor::ComponentPtr   detached;
            UndoCommandPtr        related;
            std::function<void()> changed;
        };

        class WorldPropertyCommand final : public UndoCommand {
        public:
            WorldPropertyCommand(UndoCommandPtr edit, EditContext context, const Actor &actor, const ComponentBase &component)
                : UndoCommand(edit->GetName())
                , edit(std::move(edit))
                , context(std::move(context))
                , actor(actor.GetUuid())
                , actorIncarnation(actor.GetIncarnation())
                , componentType(component.GetTypeId())
                , componentIncarnation(component.GetIncarnation())
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
                if (!Valid() || !edit->TryDo() || !Valid())
                    return false;
                context.document->MarkDirty();
                return true;
            }
            bool TryUndo() override
            {
                if (!Valid() || !edit->TryUndo() || !Valid())
                    return false;
                context.document->MarkDirty();
                return true;
            }

        private:
            bool Valid() const
            {
                if (!context.Valid())
                    return false;
                auto *owner     = context.world->GetActorByUuid(actor);
                auto *component = owner && owner->GetIncarnation() == actorIncarnation ? owner->GetComponent(componentType) : nullptr;
                return component && component->GetIncarnation() == componentIncarnation;
            }
            UndoCommandPtr edit;
            EditContext    context;
            Uuid           actor, actorIncarnation, componentType, componentIncarnation;
        };

        class ActorCommand final : public UndoCommand {
        public:
            ActorCommand(EditContext context, std::unique_ptr<Actor> actor, bool create, std::function<void()> changed)
                : UndoCommand(create ? "Create scene object" : "Delete scene object")
                , context(std::move(context))
                , detached(std::move(actor))
                , id(detached->GetUuid())
                , incarnation(detached->GetIncarnation())
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
                if (!context.Valid())
                    return false;
                auto *world = context.world.Get();
                if (attach) {
                    if (!detached || detached->GetIncarnation() != incarnation || !world->TryAttachToWorld(detached))
                        return false;
                } else if (auto *actor = world->GetActorByUuid(id)) {
                    if (actor->GetIncarnation() != incarnation)
                        return false;
                    detached = world->DetachFromWorld(actor);
                    if (!detached)
                        return false;
                } else if (!detached || detached->GetIncarnation() != incarnation) {
                    return false;
                }
                if (!context.Valid())
                    return false;
                changed();
                return true;
            }
            EditContext            context;
            std::unique_ptr<Actor> detached;
            Uuid                   id, incarnation;
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
        if (next == document && (!next || (!documentLifetime.expired() && next->GetWorld() == world.Get())))
            return;
        bindingLifetime  = std::make_shared<char>();
        documentLifetime = next ? next->GetLifetime() : std::weak_ptr<void>{};
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
        if (document && !documentLifetime.expired())
            document->MarkDirty();
    }
    UndoCommandPtr WorldEditorState::MakeEditCommand(const Uuid &owner, const PropertyDescriptor &property, Any value)
    {
        if (!document || documentLifetime.expired() || !world)
            return {};
        auto *actor = world->GetActorByUuid(owner);
        if (!actor)
            return {};
        ComponentBase *bound = nullptr;
        for (const auto &[type, component] : actor->GetComponents()) {
            const auto *node = GetTypeNode(type);
            if (node && property.IsBoundTo(component->GetReflectedObject()) &&
                std::ranges::any_of(node->members, [&](const auto &entry) { return &entry.second == property.GetMember(); })) {
                bound = component.get();
                break;
            }
        }
        if (!bound)
            return {};
        auto prepared = extensions ? extensions->MakePropertyEdit(*actor, property, value) : EditorEditResult{};
        if (prepared.state == EditorEditState::Error) {
            LOG_E("WorldEditor", "%s", prepared.error.c_str());
            return {};
        }
        auto edit = prepared.state == EditorEditState::Ready ? std::move(prepared.command) : property.MakeEditCommand(std::move(value));
        if (!edit)
            return {};
        return std::make_unique<WorldPropertyCommand>(std::move(edit), EditContext(*document, bindingLifetime), *actor, *bound);
    }
    std::vector<std::string> RegisterBuiltinActorCreators()
    {
        std::vector<std::string> ids;
        const auto               add = [&](const char *id, const char *label, std::function<bool(Actor &)> create) {
            if (EditorActorCreation::Get()->RegisterCreation({id, label, "Scene", static_cast<int>(ids.size()), std::move(create)}))
                ids.emplace_back(id);
        };
        add("empty", "Empty Object", [](Actor &actor) {
            actor.SetName("Actor");
            return true;
        });
        return ids;
    }

    bool WorldEditorState::Create(const std::string &creatorId)
    {
        const auto creator = EditorActorCreation::Get()->Find(creatorId);
        if (!world || !document || documentLifetime.expired() || !creator)
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
        auto       edit = std::make_unique<ActorCommand>(EditContext(*document, bindingLifetime), std::move(actor), true, [this]() { OnEdited({}); });
        if (!EditorCore::GetCommandService().Execute(
                std::make_unique<CreationCommand>(EditContext(*document, bindingLifetime), std::move(edit), std::move(dependencies), creator)))
            return false;
        selection.SetSelection({{SelectionType::ENTITY, id}});
        return true;
    }
    void WorldEditorState::DeleteSelection()
    {
        if (!document || documentLifetime.expired() || !world || selection.IsEmpty() ||
            selection.GetSelection().front().type != SelectionType::ENTITY)
            return;
        const auto id    = selection.GetSelection().front().id;
        auto      *actor = world->GetActorByUuid(id);
        if (!actor)
            return;
        EditContext context(*document, bindingLifetime);
        auto detached = world->DetachFromWorld(actor);
        if (!detached || !context.Valid())
            return;
        EditorCore::GetCommandService().Execute(
            std::make_unique<ActorCommand>(std::move(context), std::move(detached), false, [this]() { OnEdited({}); }));
    }

    bool WorldEditorState::CanAddComponent(const Uuid &type) const
    {
        if (!world || !document || documentLifetime.expired() || selection.GetSelection().size() != 1 ||
            selection.GetSelection().front().type != SelectionType::ENTITY || !RegisteredComponent(type))
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
        if (document && documentLifetime.expired())
            Bind(nullptr);
        else if (document && document->GetWorld() != world.Get())
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
            auto prepared = extensions ? extensions->PrepareComponentAddition(*actor, *component) : EditorEditResult{};
            if (prepared.state == EditorEditState::Error) {
                LOG_E("WorldEditor", "%s", prepared.error.c_str());
                continue;
            }
            auto edit = std::make_unique<AddComponentCommand>(EditContext(*document, bindingLifetime), *actor, std::move(component),
                                                              std::move(prepared.command), [this]() {
                                                                  OnEdited({});
                                                                  refreshSelection = true;
                                                              });
            if (!EditorCore::GetCommandService().Execute(
                    std::make_unique<CreationCommand>(EditContext(*document, bindingLifetime), std::move(edit), std::move(dependencies))))
                LOG_E("WorldEditor", "Component addition failed for actor %s, type %s", id.ToString().c_str(), type.ToString().c_str());
        }
        if (refreshSelection) {
            refreshSelection    = false;
            const auto selected = selection.GetSelection();
            selection.Clear();
            selection.SetSelection(selected);
        }
    }
} // namespace sky::editor

//
// Created on 2026/11/15.
//

#include <aurora/editor/AuroraEditorExtension.h>

#include <animation/IAnimationInstanceControl.h>
#include <aurora/adaptor/components/SkeletalMeshComponent.h>
#include <aurora/renderer/ImportedSkinBinding.h>
#include <editor/core/property/PropertyDescriptor.h>
#include <framework/asset/AssetDataBase.h>
#include <framework/interface/Interface.h>
#include <framework/world/Actor.h>
#include <framework/world/World.h>
#include <map>

namespace sky::aurora::editor {
    namespace {
        class SkeletalEditCommand final : public sky::editor::UndoCommand {
        public:
            SkeletalEditCommand(sky::Actor &actor, SkeletalMeshComponent &component, SkeletalMeshComponentData after, bool poseOnly = false)
                : UndoCommand("Edit skeletal binding")
                , world(actor.GetWorld())
                , actor(actor.GetUuid())
                , actorIncarnation(actor.GetIncarnation())
                , expected(component.GetIncarnation())
                , worldRuntime(actor.GetWorld() ? actor.GetWorld()->GetRuntimeId() : Uuid{})
                , before(component.GetData())
                , after(std::move(after))
                , poseOnly(poseOnly)
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
                return Apply(after);
            }
            bool TryUndo() override
            {
                return Apply(before);
            }

        private:
            bool Apply(const SkeletalMeshComponentData &value)
            {
                auto *owner     = world && world->GetRuntimeId() == worldRuntime ? world->GetActorByUuid(actor) : nullptr;
                auto *component = owner ? owner->GetComponent<SkeletalMeshComponent>() : nullptr;
                if (!component || owner->GetIncarnation() != actorIncarnation || component->GetIncarnation() != expected)
                    return false;
                if (poseOnly)
                    component->SetPoseMode(value.pose);
                else
                    component->SetConfig(value);
                return true;
            }
            WorldPtr                     world;
            Uuid                         actor, actorIncarnation, expected, worldRuntime;
            SkeletalMeshComponentData    before, after;
            bool                         poseOnly;
        };
    } // namespace

    sky::editor::EditorEditResult
    AuroraEditorExtension::MakePropertyEdit(sky::Actor &actor, const sky::editor::PropertyDescriptor &property, const sky::Any &value)
    {
        using sky::editor::EditorEditState;
        auto       *component = actor.GetComponent<SkeletalMeshComponent>();
        const auto *type      = component ? GetTypeNode(component->GetTypeId()) : nullptr;
        if (!component || !type || !property.IsBoundTo(component->GetReflectedObject()) || !type->members.contains("mesh") ||
            property.GetMember() != &type->members.at("mesh"))
            return {};
        const auto *mesh = value.GetAsConst<Uuid>();
        if (!mesh || !property.CanEdit() || !actor.GetWorld())
            return {EditorEditState::Error, {}, "Skeletal mesh editing requires a writable UUID field on a live actor"};
        auto after = component->GetData();
        after.mesh = *mesh;
        if (!after.skin && !after.skeleton) {
            Uuid                        source;
            std::map<std::string, Uuid> defaults;
            ImportedSkinBinding         binding;
            auto                       *database = AssetDataBase::Get();
            if (database->GetOwner(*mesh, source) && database->GetGeneratedDefaults(source, defaults) &&
                ResolveImportedSkinBinding(*mesh, defaults, binding) == ImportedSkinBindingStatus::Ready) {
                after.skin     = binding.skin;
                after.skeleton = binding.skeleton;
            }
        }
        return {EditorEditState::Ready, std::make_unique<SkeletalEditCommand>(actor, *component, std::move(after)), {}};
    }

    sky::editor::EditorEditResult AuroraEditorExtension::PrepareComponentAddition(sky::Actor &actor, sky::ComponentBase &component)
    {
        using sky::editor::EditorEditState;
        auto *control  = Interface<IAnimationInstanceControl>::Get()->GetApi();
        auto *skeletal = control && control->GetComponentType() == component.GetTypeId() ? actor.GetComponent<SkeletalMeshComponent>() : nullptr;
        if (!skeletal)
            return {};
        const auto *type = GetTypeNode(component.GetTypeId());
        if (component.GetActor() || !actor.GetWorld() || !type || !component.GetReflectedObject() || !type->members.contains("skeleton"))
            return {EditorEditState::Error, {}, "Animation authoring requires a detached component with a Skeleton UUID"};
        const auto                     &member = type->members.at("skeleton");
        sky::editor::PropertyDescriptor skeleton(component.GetReflectedObject(), &member, "skeleton", "");
        if (!skeleton.GetType() || skeleton.GetType()->registeredId != TypeInfo<Uuid>::RegisteredId() || !skeleton.CanEdit() ||
            !skeleton.SetValue(Any(skeletal->GetSkeletonUuid())))
            return {EditorEditState::Error, {}, "Animation authoring component has no writable Skeleton UUID"};
        auto after = skeletal->GetData();
        after.pose = SkeletalPoseMode::SameActorAnimation;
        return {EditorEditState::Ready, std::make_unique<SkeletalEditCommand>(actor, *skeletal, std::move(after), true), {}};
    }
} // namespace sky::aurora::editor

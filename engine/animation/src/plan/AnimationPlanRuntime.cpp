//
// Created by blues on 2026/9/21.
//

#include <animation/plan/AnimationPlanRuntime.h>
#include <animation/graph/AnimationClipNode.h>
#include <animation/graph/PoseBlendNode.h>
#include <animation/core/Skeleton.h>

namespace sky {

    AnimationPlanRuntime::~AnimationPlanRuntime() = default;

    bool AnimationPlanRuntime::Compile(AnimNode* root, const Skeleton& skeleton)
    {
        diagnostics.clear();
        clipBindings.clear();
        blendBindings.clear();
        listBindings.clear();
        stateBindings.clear();
        compiled = false;

        std::vector<Transform> bindPose = skeleton.GetRefPos()->transforms;
        AnimationPlanBuilder builder(static_cast<uint32_t>(skeleton.GetNumBones()), bindPose);

        if (!skeleton.GetRoots().empty()) {
            builder.SetRootBone(skeleton.GetRoots().front()->index);
        }

        AnimPlanLowerInfo info;
        info.skeleton = &skeleton;

        uint32_t outputSlot = ANIM_INVALID_SLOT;
        if (root == nullptr || !root->LowerToPlan(builder, info, outputSlot)) {
            diagnostics = builder.GetDiagnostics();
            if (diagnostics.empty()) {
                diagnostics.emplace_back("plan lowering failed");
            }
            return false;
        }

        builder.SetOutputSlot(outputSlot);

        AnimOpRecord outputOp;
        outputOp.op = AnimOp::Output;
        outputOp.output = static_cast<uint16_t>(outputSlot);
        builder.EmitOp(outputOp);

        if (!builder.Build(plan)) {
            diagnostics = builder.GetDiagnostics();
            return false;
        }

        clipBindings = builder.GetClipBindings();
        blendBindings = builder.GetBlendBindings();
        listBindings = builder.GetListBindings();
        stateBindings = builder.GetStateBindings();

        frameState.Resize(plan.GetNumTimeSlots(), plan.GetNumWeightSlots(), plan.GetNumOps());
        compiled = true;
        return true;
    }

    void AnimationPlanRuntime::Init()
    {
        AnimContext context{};

        for (AnimationClipBinding& binding : clipBindings) {
            if (binding.node != nullptr) {
                binding.node->InitAny(context);
            }
        }

        for (AnimationStateBinding& binding : stateBindings) {
            if (binding.node != nullptr) {
                binding.node->InitAny(context);
            }
        }

        UpdateStateOps();
    }

    void AnimationPlanRuntime::Tick(float deltaTime)
    {
        AnimLayerContext context{};

        for (AnimationClipBinding& binding : clipBindings) {
            AnimationClipNode* node = binding.node;
            if (node == nullptr) {
                continue;
            }

            node->AdvanceControl(deltaTime);
            frameState.clipTimes[binding.timeSlot] = node->GetPlayerTime();

            if (binding.rootMotionSlot < frameState.rootMotion.size()) {
                frameState.rootMotion[binding.rootMotionSlot] = node->IsRootMotionEnable() ? 1 : 0;
            }
        }

        for (AnimationBlendBinding& binding : blendBindings) {
            PoseBlend2Node* node = binding.node;
            if (node == nullptr) {
                continue;
            }

            node->UpdateBlendControl(deltaTime);
            frameState.weights[binding.weightSlot] = node->GetBlendedAlpha();
        }

        for (AnimationListBinding& binding : listBindings) {
            PoseBlendNodeList* node = binding.node;
            if (node == nullptr) {
                continue;
            }

            float total = 0.f;
            for (uint32_t poseIndex : binding.poseIndices) {
                total += node->GetPoseWeight(poseIndex);
            }

            const size_t count = binding.poseIndices.size();
            for (size_t i = 0; i < count; ++i) {
                const float weight = node->GetPoseWeight(binding.poseIndices[i]);
                if (binding.weightSlots[i] < frameState.weights.size()) {
                    frameState.weights[binding.weightSlots[i]] = total > 0.f ? weight / total : 0.f;
                }
            }
        }

        for (AnimationStateBinding& binding : stateBindings) {
            if (binding.node != nullptr) {
                binding.node->UpdateControl(context, deltaTime);
            }
        }

        UpdateStateOps();
    }

    void AnimationPlanRuntime::UpdateStateOps()
    {
        if (stateBindings.empty()) {
            return;
        }

        frameState.SetAllOpsEnabled(plan.GetNumOps());

        for (AnimationStateBinding& binding : stateBindings) {
            if (binding.node == nullptr) {
                continue;
            }

            for (const AnimStateOpRange& range : binding.stateOpRanges) {
                frameState.DisableOps(range.firstOp, range.lastOp);
            }

            const AnimHandle active = binding.node->GetCurrentStateHandle();
            if (active < binding.stateOpRanges.size()) {
                const AnimStateOpRange& range = binding.stateOpRanges[active];
                frameState.EnableOps(range.firstOp, range.lastOp);
            }

            frameState.activeState = active;
        }
    }

    void AnimationPlanRuntime::Evaluate(AnimPose& out, Transform* outRootMotionDelta) const
    {
        plan.Evaluate(frameState, out, outRootMotionDelta);
    }

} // namespace sky

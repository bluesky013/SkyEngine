//
// Created by blues on 2026/9/21.
//

#pragma once

#include <animation/plan/AnimationPlan.h>

#include <string>
#include <vector>

namespace sky {

    class AnimNode;
    class Skeleton;

    /**
     * Compiles an authoring graph into an evaluation plan and drives the control plane.
     * Control: clip time advance, blend weight, state transitions, root-motion flags.
     * Data: AnimationPlan::Evaluate reads the resulting AnimFrameState only.
     */
    class AnimationPlanRuntime {
    public:
        AnimationPlanRuntime() = default;
        ~AnimationPlanRuntime();

        AnimationPlanRuntime(const AnimationPlanRuntime&) = delete;
        AnimationPlanRuntime& operator=(const AnimationPlanRuntime&) = delete;

        bool Compile(AnimNode* root, const Skeleton& skeleton);

        void Init();
        void Tick(float deltaTime);
        void Evaluate(AnimPose& out, Transform* outRootMotionDelta = nullptr) const;

        const AnimationPlan& GetPlan() const { return plan; }
        const AnimFrameState& GetFrameState() const { return frameState; }
        const std::vector<std::string>& GetDiagnostics() const { return diagnostics; }
        std::string Dump() const { return plan.Dump(); }

    private:
        void UpdateStateOps();

        AnimationPlan plan;
        AnimFrameState frameState;

        std::vector<AnimationClipBinding> clipBindings;
        std::vector<AnimationBlendBinding> blendBindings;
        std::vector<AnimationListBinding> listBindings;
        std::vector<AnimationStateBinding> stateBindings;

        std::vector<std::string> diagnostics;
        bool compiled = false;
    };

} // namespace sky

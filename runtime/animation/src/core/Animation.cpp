//
// Created by blues on 2024/8/2.
//

#include <animation/core/Animation.h>

namespace sky {

    AnimationAsyncContext::AnimationAsyncContext(Animation* inAnim)
        : instance(inAnim)
    {
    }

    void AnimationAsyncContext::InitRoot(AnimNode* root)
    {
        rootNode = root;
        if (rootNode != nullptr) {
            AnimContext context{this};
            rootNode->InitAny(context);
        }
    }

    void AnimationAsyncContext::PreTick(const AnimationTick& tick)
    {
        // fetch events
        currentDelta = tick.deltaTime;

        if (rootNode != nullptr) {
            rootNode->PreTick(tick);
        }
    }

    void AnimationAsyncContext::UpdateAny()
    {
        if (rootNode != nullptr) {
            AnimLayerContext context{this};
            rootNode->TickAny(context, currentDelta);
        }
    }

    void AnimationAsyncContext::EvalAny(AnimationEval& context)
    {
        if (rootNode != nullptr) {
            rootNode->EvalAny(context);
        } else {
            context.pose.ResetRefPose();
        }
    }

    Animation::Animation(AnimationAsyncContext* context)
        : asyncContext(context)
    {
    }

    Animation::~Animation()
    {
        asyncContext = nullptr;
        nodeStorages.clear();
    }

    void Animation::StopAndReset()
    {
    }

    void Animation::Init(const AnimationInit& init)
    {
        StopAndReset();

        asyncContext->InitRoot(init.rootNode);
    }

    void Animation::Tick(const AnimationTick& tick)
    {
        // pre update
        updated = false;
        asyncContext->PreTick(tick);

        // update
        OnTick(tick.deltaTime);

        // post update
        if (!UseAsync()) {
            UpdateAny();
            updated = true;
        }
    }

    void Animation::UpdateAny()
    {
        asyncContext->UpdateAny();
    }

    bool Animation::UseAsync() const // NOLINT
    {
        return false;
    }

} // namespace sky

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
            AnimContext context{instance};
            rootNode->InitAsync(context);
        }
    }

    void AnimationAsyncContext::PreTick(const AnimationTick& tick)
    {
        // fetch events
        currentDelta = tick.deltaTime;
    }

    void AnimationAsyncContext::UpdateAny()
    {

    }

    void AnimationAsyncContext::EvalAny()
    {

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
        asyncContext->PreTick(tick);

        // update
        OnTick(tick.deltaTime);

        // post update
        if (!UseAsync()) {
            asyncContext->UpdateAny();
        }

        // post tick
        OnPostTick();
    }

    bool Animation::UseAsync() const // NOLINT
    {
        return false;
    }

} // namespace sky

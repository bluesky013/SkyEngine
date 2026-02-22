//
// Created by blues on 2024/8/2.
//

#pragma once

#include <core/template/ReferenceObject.h>
#include <animation/core/AnimationClip.h>
#include <animation/graph/AnimationNode.h>
#include <unordered_map>

namespace sky {

    class Animation;

    struct AnimationInit {
        AnimNode* rootNode = nullptr;
    };

    struct AnimationTick {
        float deltaTime = 0.f;
    };

    struct AnimationParameterResult {
        const uint8_t* data = nullptr;
#if ENABLE_ANIM_CHECK
        uint32_t size = 0;
#endif
    };

    class AnimationAsyncContext {
    public:
        explicit AnimationAsyncContext(Animation* inAnim);
        virtual ~AnimationAsyncContext() = default;

        void InitRoot(AnimNode* root);
        virtual void PreTick(const AnimationTick& tick);

        virtual void UpdateAny();
        virtual void EvalAny(AnimationEval& context);

    private:
        Animation* instance = nullptr;
        AnimNode* rootNode = nullptr;

        // frame init data
        float currentDelta = 0.f;
    };

    /**
     * Animation Instance
     */
    class Animation : public RefObject {
    public:
        explicit Animation(AnimationAsyncContext* context);
        ~Animation() override;

        template <typename T, typename ...Args>
        T* NewAnimNode(Args&&...args)
        {
            nodeStorages.emplace_back(new T(std::forward<Args>(args)...));
            return static_cast<T*>(nodeStorages.back().get());
        }

        template <class T ,typename ...Args>
        T* NewParameter(const Name& name, Args&& ...args)
        {
            auto *res = new T(std::forward<Args>(args)...);
            parameters[name].reset(res);
            return res;
        }

        void RemoveParameter(const Name& name)
        {
            parameters.erase(name);
        }

        void Init(const AnimationInit& init);

        void StopAndReset();

        void Tick(const AnimationTick& tick);

        // parallel
        void UpdateAny();

        virtual void EvalAny(AnimationEval& eval) {}
    protected:
        virtual void OnTick(float delta) {}
        virtual bool UseAsync() const;

        std::unique_ptr<AnimationAsyncContext> asyncContext;
        std::vector<std::unique_ptr<AnimNode>> nodeStorages;
        std::unordered_map<Name, std::unique_ptr<IAnimParameter>> parameters;

        bool updated = false;
    };
    using AnimationPtr = CounterPtr<Animation>;

} // namespace sky

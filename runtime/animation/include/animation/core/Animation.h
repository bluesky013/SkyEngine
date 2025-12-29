//
// Created by blues on 2024/8/2.
//

#pragma once

#include <core/template/ReferenceObject.h>
#include <animation/core/AnimationClip.h>
#include <animation/graph/AnimationNode.h>
#include <unordered_map>

namespace sky {

    class AnimParameters {
    public:
        template <typename T>
        void AddParameters(const Name &name, const T& val)
        {
            if constexpr (std::is_same_v<T, bool>) {
                new (AppendValue(name, sizeof(bool), AnimParamType::BOOL)) T(val);
            } else if constexpr (std::is_same_v<T, uint32_t>) {
                new (AppendValue(name, sizeof(uint32_t), AnimParamType::UINT)) T(val);
            } else if constexpr (std::is_same_v<T, int32_t>) {
                new (AppendValue(name, sizeof(int32_t), AnimParamType::INT)) T(val);
            } else if constexpr (std::is_same_v<T, float>) {
                new (AppendValue(name, sizeof(float), AnimParamType::FLOAT)) T(val);
            } else {
                static_assert("animation parameter not supported");
            }
        }

        template <typename T>
        T* GetParameters(const Name& name)
        {
            auto iter = paramMap.find(name);
            if (iter == paramMap.end()) {
                return nullptr;
            }

            uint8_t *res = nullptr;
            if constexpr (std::is_same_v<T, bool>) {
                res = GetValue(iter->second, AnimParamType::BOOL);
            } else if constexpr (std::is_same_v<T, uint32_t>) {
                res = GetValue(iter->second, AnimParamType::UINT);
            } else if constexpr (std::is_same_v<T, int32_t>) {
                res = GetValue(iter->second, AnimParamType::INT);
            } else if constexpr (std::is_same_v<T, float>) {
                res = GetValue(iter->second, AnimParamType::FLOAT);
            } else {
                static_assert("animation parameter not supported");
            }

            return reinterpret_cast<T*>(res);
        }


    private:
        uint8_t *AppendValue(const Name& val, uint32_t length, AnimParamType type)
        {
            size_t dataLen = length + sizeof(AnimParamType);
            size_t current = storage.size();
            storage.resize(dataLen + current);
            paramMap.emplace(val, static_cast<uint32_t>(current));

            uint8_t *ptr = &storage[current];
            new (ptr) AnimParamType(type);

            ptr += sizeof(AnimParamType);
            return ptr;
        }

        uint8_t *GetValue(uint32_t offset, AnimParamType type)
        {
            uint8_t *ptr = &storage[offset];

            if (*reinterpret_cast<AnimParamType*>(ptr) == type) {
                return ptr + sizeof(AnimParamType);
            }

            return nullptr;
        }

        std::vector<uint8_t> storage;
        std::unordered_map<Name, uint32_t> paramMap;
    };

    class Animation;

    struct AnimationInit {
        AnimNode* rootNode = nullptr;
    };

    struct AnimationTick {
        float deltaTime = 0.f;
    };

    struct AnimationEval {
        AnimPose& outPose;
    };

    class AnimationAsyncContext {
    public:
        explicit AnimationAsyncContext(Animation* inAnim);
        virtual ~AnimationAsyncContext() = default;

        void InitRoot(AnimNode* root);
        virtual void PreTick(const AnimationTick& tick);

        virtual void UpdateAny();
        virtual void EvalAny(PoseContext& context);

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
        AnimNode* NewAnimNode(Args&&...args)
        {
            nodeStorages.emplace_back(new T(std::forward<Args>(args)...));
            return nodeStorages.back().get();
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

        bool updated = false;
    };
    using AnimationPtr = CounterPtr<Animation>;

} // namespace sky

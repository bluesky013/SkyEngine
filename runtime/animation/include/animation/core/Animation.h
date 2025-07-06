//
// Created by blues on 2024/8/2.
//

#pragma once

#include <core/template/ReferenceObject.h>
#include <animation/core/AnimationClip.h>
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

    class Animation : public RefObject {
    public:
        Animation() = default;
        ~Animation() override = default;

        virtual void Tick(float delta) = 0;

        void AddClip(const AnimClipPtr& clip);

    protected:
        std::vector<AnimClipPtr> clips;
    };
    using AnimationPtr = CounterPtr<Animation>;

    class KeyFrameAnimation : public Animation {
    public:
        KeyFrameAnimation() = default;
        ~KeyFrameAnimation() override = default;

        void Tick(float delta) override;

    protected:
        virtual void Sample(AnimationClip& clip, float timePoint) = 0;

        float currentTime = 0.f;
        size_t currentClip = 0;
    };

} // namespace sky

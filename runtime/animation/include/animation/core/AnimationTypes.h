//
// Created by blues on 2024/8/1.
//

#pragma once

#include <core/platform/Platform.h>
#include <core/name/Name.h>
#include <core/util/Cast.h>
#include <core/math/Math.h>
#include <algorithm>
#include <vector>

namespace sky {

    static constexpr uint32_t ANIM_INVALID = ~(0U);
    static constexpr uint32_t ANIM_MAX_BONE = 256;

    static constexpr float ANIM_BLEND_WEIGHT_THRESHOLD = 0.00001f;
    static constexpr float ANIM_DIFF_TOLERANCE = static_cast<float>(1e-6);
    static constexpr float ANIM_INV_BLEND_WEIGHT_THRESHOLD = 1.f - ANIM_BLEND_WEIGHT_THRESHOLD;

    enum class AnimInterpolation : uint8_t {
        LINEAR,
        STEP,
        CUBIC_SPLINE,
    };

    enum class AnimBlendMode : uint8_t {
        Override,
        Additive,
        Multiply,
        Replace
    };

    enum class AnimLayerUpdateMode : uint8_t {
        Always,
        WhenActive,
        Manual
    };

    enum class AnimParamType : uint8_t {
        UNKNOWN = 0,
        BOOL,
        UINT,
        INT,
        FLOAT,
        DOUBLE,
        STR
    };

    enum class AnimComp : uint8_t {
        NEV = 0,
        LT  = 1,
        EQ  = 2,
        LE  = 3,
        GT  = 4,
        NE  = 5,
        GE  = 6,
        AWS = 7,
        NUM
    };

    using AnimTimeKey = int32_t;

    struct AnimFrameTime {
        AnimTimeKey frame;
        float subFrame;

        explicit operator float() const
        {
            return static_cast<float>(frame) + subFrame;
        }
    };

    struct SampleParam {
        AnimFrameTime frameTime;
        AnimInterpolation interpolation;
    };

    template <typename T>
    struct AnimChannelData {
        std::pair<size_t, size_t> FindKeyFrame(AnimTimeKey time) const
        {
            auto it = std::ranges::upper_bound(times,time);
            if (it == times.begin()) {
                return {0, 0};
            }

            size_t last = times.size() - 1;
            if (it == times.end()) {
                return { last, last};
            }

            size_t idx = std::distance(times.begin(), it);
            return { idx - 1, idx };
        }

        static constexpr size_t VecNum = VectorTraits<T>::Size;

        void Compress()
        {
            // no need to compress
            if (keys.size() <= 1) {
                return;
            }

            std::vector<AnimTimeKey> compressedTimes;
            std::vector<T> compressedKeys;

            compressedTimes.emplace_back(times[0]);
            compressedKeys.emplace_back(keys[0]);

            for (size_t i = 1; i < keys.size(); ++i) {
                bool isTolerate = false;
                for (size_t j = 0; j < VecNum; ++j) {
                    if (std::abs(VectorTraits<T>::Visit(keys[i], j) - VectorTraits<T>::Visit(compressedKeys.back(), j)) >= ANIM_DIFF_TOLERANCE) {
                        isTolerate = true;
                        break;
                    }
                }

                if (isTolerate) {
                    compressedTimes.emplace_back(times[i]);
                    compressedKeys.emplace_back(keys[i]);
                }
            }

            times.swap(compressedTimes);
            keys.swap(compressedKeys);
        }

        void Resize(const size_t size)
        {
            times.resize(size);
            keys.resize(size);
        }

        std::vector<AnimTimeKey> times;
        std::vector<T> keys;
    };

    template <typename T>
    class AnimCompEval {
    public:
        using CompFunc = bool(*)(const T&, const T&);

        static bool NEV(const T& a, const T& b) { return false; }
        static bool LT (const T& a, const T& b) { return a < b; }
        static bool EQ (const T& a, const T& b) { return a == b; }
        static bool LE (const T& a, const T& b) { return a <= b; }
        static bool GT (const T& a, const T& b) { return a > b; }
        static bool NE (const T& a, const T& b) { return a != b; }
        static bool GE (const T& a, const T& b) { return a >= b; }
        static bool AWS(const T& a, const T& b) { return true; }

        static bool Compare(AnimComp comp, const T& a, const T& b)
        {
            return GetFuncArray()[ToSize(comp)](a, b);
        }

    private:
        static const CompFunc* GetFuncArray() {
            static const CompFunc funcs[] = {
                &AnimCompEval::NEV,
                &AnimCompEval::LT,
                &AnimCompEval::EQ,
                &AnimCompEval::LE,
                &AnimCompEval::GT,
                &AnimCompEval::NE,
                &AnimCompEval::GE,
                &AnimCompEval::AWS
            };
            return funcs;
        }

    };

    struct IAnimTransCond {
        IAnimTransCond() = default;
        virtual ~IAnimTransCond() = default;

        virtual void Update(float) = 0;
        virtual bool Eval() const = 0;
    };

    struct IAnimParameter {
        IAnimParameter() = default;
        virtual ~IAnimParameter() = default;

        virtual void Update(float deltaTime) = 0;
        virtual const uint8_t *Eval() const = 0;

        template <typename T>
        const T& EvalAs() const
        {
            return *reinterpret_cast<const T*>(Eval());
        }
    };

    template <typename T>
    struct TAnimParameter : IAnimParameter {
        T cachedValue {};

        const uint8_t *Eval() const override
        {
            return reinterpret_cast<const uint8_t *>(&cachedValue);
        }
    };

    template <typename T>
    struct TAnimRefCachedParameter : TAnimParameter<T> {
        const T& val;

        explicit TAnimRefCachedParameter(T& inVal) : val(inVal) {}

        void Update(float deltaTime) override
        {
            this->cachedValue = val;
        }

    };

    template <typename T>
    struct TAnimFuncParameter : TAnimParameter<T> {
        std::function<T(float)> func;

        explicit TAnimFuncParameter(std::function<T(float)>&& fn)
            : func(fn)
        {
        }

        void Update(float deltaTime) override
        {
            this->cachedValue = func(deltaTime);
        }
    };

} // namespace sky

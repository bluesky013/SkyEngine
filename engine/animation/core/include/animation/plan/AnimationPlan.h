//
// Created by blues on 2026/9/21.
//

#pragma once

#include <animation/core/AnimationPose.h>
#include <animation/core/AnimationTrackData.h>
#include <animation/core/AnimationTypes.h>
#include <animation/core/AnimationUtils.h>
#include <animation/graph/AnimationState.h>

#include <cstdint>
#include <string>
#include <vector>

namespace sky {

    class AnimationClipNode;
    class PoseBlend2Node;
    class PoseBlendNodeList;
    class AnimStateMachine;
    class Skeleton;

    static constexpr uint16_t ANIM_INVALID_SLOT = 0xFFFF;

    enum class AnimOp : uint8_t {
        ClipSample    = 0,
        Blend         = 1,
        AdditiveBlend = 2,
        Layer         = 3,
        StateSelect   = 4,
        Output        = 5,
    };

    struct AnimOpRecord {
        AnimOp   op            = AnimOp::Output;
        uint16_t inputA        = ANIM_INVALID_SLOT;
        uint16_t inputB        = ANIM_INVALID_SLOT;
        uint16_t output        = ANIM_INVALID_SLOT;
        uint16_t weightIndex   = ANIM_INVALID_SLOT;
        uint16_t timeIndex     = ANIM_INVALID_SLOT;
        uint16_t boneMaskIndex = ANIM_INVALID_SLOT;
        float    weight        = 0.f;
        uint32_t dataIndex     = 0;
    };

    /**
     * Control-plane output per frame. Preallocated and reused; the data plane only reads it.
     */
    struct AnimFrameState {
        AnimHandle activeState = ANIM_INVALID_HANDLE;

        std::vector<float>   clipTimes;   // indexed by op.timeIndex
        std::vector<float>   weights;     // indexed by op.weightIndex
        std::vector<uint8_t> rootMotion;  // indexed by op.weightIndex (1 = enabled)
        std::vector<uint8_t> opEnabled;   // indexed by op index (1 = evaluated)

        void Resize(uint32_t numTimeSlots, uint32_t numWeightSlots, uint32_t numOps)
        {
            clipTimes.resize(numTimeSlots, 0.f);
            weights.resize(numWeightSlots, 0.f);
            rootMotion.resize(numWeightSlots, 1);
            opEnabled.assign(numOps, 1);
        }

        void SetAllOpsEnabled(uint32_t numOps)
        {
            opEnabled.assign(numOps, 1);
        }

        void DisableOps(uint32_t firstOp, uint32_t lastOp)
        {
            for (uint32_t i = firstOp; i <= lastOp && i < opEnabled.size(); ++i) {
                opEnabled[i] = 0;
            }
        }

        void EnableOps(uint32_t firstOp, uint32_t lastOp)
        {
            for (uint32_t i = firstOp; i <= lastOp && i < opEnabled.size(); ++i) {
                opEnabled[i] = 1;
            }
        }
    };

    /**
     * Flattened, topologically ordered evaluation plan over a preallocated pose pool.
     * Pointer-free: all cross references are indices/offsets.
     */
    class AnimationPlan {
    public:
        static constexpr uint32_t LayoutVersion = 1;

        void Evaluate(const AnimFrameState& state, AnimPose& out, Transform* outRootMotionDelta = nullptr) const;

        uint32_t GetNumSlots() const { return numSlots; }
        uint32_t GetNumBones() const { return numBones; }
        uint32_t GetOutputSlot() const { return outputSlot; }
        uint32_t GetNumTimeSlots() const { return numTimeSlots; }
        uint32_t GetNumWeightSlots() const { return numWeightSlots; }
        uint32_t GetNumClips() const { return static_cast<uint32_t>(clips.size()); }
        uint32_t GetNumStateSlots() const { return static_cast<uint32_t>(stateSlots.size()); }
        uint32_t GetNumBoneMasks() const { return static_cast<uint32_t>(boneMasks.size()); }
        uint32_t GetNumOps() const { return static_cast<uint32_t>(ops.size()); }
        bool HasRoot() const { return hasRoot; }
        uint16_t GetRootBone() const { return rootBone; }

        const std::vector<AnimOpRecord>& GetOps() const { return ops; }
        const std::vector<AnimationTrackData>& GetClips() const { return clips; }
        const std::vector<float>& GetClipFrameRates() const { return clipFrameRates; }
        const std::vector<Transform>& GetBindPose() const { return bindPose; }
        const std::vector<AnimationBoneMask>& GetBoneMasks() const { return boneMasks; }

        std::string Dump() const;

    private:
        friend class AnimationPlanBuilder;
        friend struct AnimationPlanBlob;

        std::vector<AnimOpRecord> ops;
        mutable std::vector<Transform> posePool;
        std::vector<Transform> bindPose;
        std::vector<AnimationTrackData> clips;
        std::vector<float> clipFrameRates;
        std::vector<AnimationBoneMask> boneMasks;
        std::vector<uint16_t> stateSlots;

        uint32_t numSlots = 0;
        uint32_t numBones = 0;
        uint32_t outputSlot = ANIM_INVALID_SLOT;
        uint32_t numTimeSlots = 0;
        uint32_t numWeightSlots = 0;
        uint16_t rootBone = 0;
        bool hasRoot = false;
    };

    // ---- Runtime (control-plane) bindings. Never serialized. ----

    struct AnimPlanLowerInfo {
        const Skeleton* skeleton = nullptr;
    };

    struct AnimationClipBinding {
        AnimationClipNode* node = nullptr;
        uint32_t timeSlot = 0;
        uint32_t rootMotionSlot = ANIM_INVALID_SLOT;
    };

    struct AnimationBlendBinding {
        PoseBlend2Node* node = nullptr;
        uint32_t weightSlot = 0;
    };

    struct AnimationListBinding {
        PoseBlendNodeList* node = nullptr;
        std::vector<uint32_t> poseIndices;
        std::vector<uint32_t> weightSlots;
    };

    struct AnimStateOpRange {
        uint32_t firstOp = 0;
        uint32_t lastOp = 0;
    };

    struct AnimationStateBinding {
        AnimStateMachine* node = nullptr;
        std::vector<AnimStateOpRange> stateOpRanges;
    };

    class AnimationPlanBuilder {
    public:
        AnimationPlanBuilder(uint32_t numBones, const std::vector<Transform>& inBindPose);

        uint32_t AllocateSlot();
        uint32_t AddClip(const AnimationTrackData& data, float frameRate);
        uint32_t AddBoneMask(const AnimationBoneMask& mask);
        uint32_t AddTimeSlot();
        uint32_t AddWeightSlot();
        uint32_t AddStateTable(const std::vector<uint16_t>& slots);
        uint32_t EmitOp(const AnimOpRecord& op);

        void SetOutputSlot(uint32_t slot) { outputSlot = slot; }
        void SetRootBone(uint16_t bone) { rootBone = bone; hasRoot = true; }

        uint32_t GetNumSlots() const { return numSlots; }
        uint32_t GetNumTimeSlots() const { return numTimeSlots; }
        uint32_t GetNumWeightSlots() const { return numWeightSlots; }
        uint32_t GetOpCount() const { return static_cast<uint32_t>(ops.size()); }

        void AddClipBinding(const AnimationClipBinding& binding) { clipBindings.emplace_back(binding); }
        void AddBlendBinding(const AnimationBlendBinding& binding) { blendBindings.emplace_back(binding); }
        void AddListBinding(const AnimationListBinding& binding) { listBindings.emplace_back(binding); }
        void AddStateBinding(const AnimationStateBinding& binding) { stateBindings.emplace_back(binding); }

        const std::vector<AnimationClipBinding>& GetClipBindings() const { return clipBindings; }
        const std::vector<AnimationBlendBinding>& GetBlendBindings() const { return blendBindings; }
        const std::vector<AnimationListBinding>& GetListBindings() const { return listBindings; }
        const std::vector<AnimationStateBinding>& GetStateBindings() const { return stateBindings; }

        const std::vector<std::string>& GetDiagnostics() const { return diagnostics; }
        void AddError(const std::string& msg) { diagnostics.emplace_back(msg); }

        bool Validate();
        bool Build(AnimationPlan& out);

    private:
        uint32_t numBones = 0;
        uint32_t numSlots = 0;
        uint32_t numTimeSlots = 0;
        uint32_t numWeightSlots = 0;
        uint32_t outputSlot = ANIM_INVALID_SLOT;
        uint16_t rootBone = 0;
        bool hasRoot = false;

        std::vector<AnimOpRecord> ops;
        std::vector<AnimationTrackData> clips;
        std::vector<float> clipFrameRates;
        std::vector<Transform> bindPose;
        std::vector<AnimationBoneMask> boneMasks;
        std::vector<uint16_t> stateSlots;
        std::vector<std::string> diagnostics;

        std::vector<AnimationClipBinding> clipBindings;
        std::vector<AnimationBlendBinding> blendBindings;
        std::vector<AnimationListBinding> listBindings;
        std::vector<AnimationStateBinding> stateBindings;
    };

    /**
     * Pointer-free binary layout. All cross references are counts / packed offsets,
     * so the blob can be bound directly as a GPU buffer. Layout (little-endian):
     *
     *   Header:
     *     u32 magic ('ANPL') | u32 version | u32 numSlots | u32 numBones
     *     u32 outputSlot      | u32 numTimeSlots | u32 numWeightSlots
     *     u16 rootBone        | u8 hasRoot | u8 pad
     *     u32 numOps          | u32 numClips | u32 numBoneMasks
     *   Ops:        numOps * sizeof(AnimOpRecord)
     *   BindPose:   numBones * sizeof(Transform)
     *   BoneMasks:  numBoneMasks * sizeof(AnimationBoneMask)
     *   StateSlots: u32 count | count * u16
     *   Per clip:
     *     u32 numTracks | u32 numTimes | u32 numTranslations | u32 numScales
     *     u32 numRotations | f32 frameRate
     *     tracks[numTracks] | times[numTimes]
     *     translations[numTranslations] | scales[numScales] | rotations[numRotations]
     *
     * AnimationTrack.timeOffset/valueOffset index into the clip's blocks, so the
     * value blocks and time block stay independent of track ordering.
     * Control-plane bindings (node pointers) are runtime-only and never serialized.
     */
    struct AnimationPlanBlob {
        static std::vector<uint8_t> Serialize(const AnimationPlan& plan);
        static bool Deserialize(const std::vector<uint8_t>& data, AnimationPlan& out);
    };

} // namespace sky

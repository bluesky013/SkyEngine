//
// Created by blues on 2026/9/21.
//

#include <animation/plan/AnimationPlan.h>

#include <cstring>

namespace sky {

    namespace {

        float EffectiveWeight(const AnimOpRecord& op, const AnimFrameState& state)
        {
            if (op.weightIndex != ANIM_INVALID_SLOT && op.weightIndex < state.weights.size()) {
                return state.weights[op.weightIndex];
            }
            return op.weight;
        }

        const char* OpName(AnimOp op)
        {
            switch (op) {
            case AnimOp::ClipSample:    return "ClipSample";
            case AnimOp::Blend:         return "Blend";
            case AnimOp::AdditiveBlend: return "AdditiveBlend";
            case AnimOp::Layer:         return "Layer";
            case AnimOp::StateSelect:   return "StateSelect";
            case AnimOp::Output:        return "Output";
            }
            return "?";
        }

    } // namespace

    void AnimationPlan::Evaluate(const AnimFrameState& state, AnimPose& out, Transform* outRootMotionDelta) const
    {
        if (posePool.empty() || numBones == 0 || numSlots == 0) {
            return;
        }

        Transform* pool = posePool.data();
        const size_t boneBytes = static_cast<size_t>(numBones) * sizeof(Transform);

        for (uint32_t opIndex = 0; opIndex < static_cast<uint32_t>(ops.size()); ++opIndex) {
            if (opIndex < state.opEnabled.size() && state.opEnabled[opIndex] == 0) {
                continue;
            }

            const AnimOpRecord& op = ops[opIndex];
            if (op.output >= numSlots) {
                continue;
            }

            Transform* dst = pool + static_cast<size_t>(op.output) * numBones;

            switch (op.op) {
            case AnimOp::ClipSample: {
                if (op.dataIndex >= clips.size()) {
                    break;
                }

                std::memcpy(dst, bindPose.data(), boneBytes);

                const AnimationBoneMask* mask =
                    (op.boneMaskIndex != ANIM_INVALID_SLOT && op.boneMaskIndex < boneMasks.size())
                        ? &boneMasks[op.boneMaskIndex]
                        : nullptr;

                const float time = (op.timeIndex != ANIM_INVALID_SLOT && op.timeIndex < state.clipTimes.size())
                    ? state.clipTimes[op.timeIndex]
                    : 0.f;

                SampleParam param = {};
                param.frameTime = Anim::ConvertFromFrameRate(time, op.weight);
                param.interpolation = AnimInterpolation::LINEAR;
                clips[op.dataIndex].SamplePose(dst, param, mask);

                if (hasRoot && rootBone < numBones) {
                    const bool rootMotion = (op.weightIndex == ANIM_INVALID_SLOT ||
                                             op.weightIndex >= state.rootMotion.size() ||
                                             state.rootMotion[op.weightIndex] != 0);

                    if (outRootMotionDelta != nullptr) {
                        *outRootMotionDelta = bindPose[rootBone].GetInverse() * dst[rootBone];
                    }

                    if (!rootMotion) {
                        dst[rootBone] = bindPose[rootBone];
                    }
                }
                break;
            }
            case AnimOp::Blend:
            case AnimOp::AdditiveBlend: {
                if (op.inputA >= numSlots || op.inputB >= numSlots) {
                    break;
                }

                const Transform* a = pool + static_cast<size_t>(op.inputA) * numBones;
                const Transform* b = pool + static_cast<size_t>(op.inputB) * numBones;
                const float weight = EffectiveWeight(op, state);

                if (op.op == AnimOp::AdditiveBlend) {
                    for (uint32_t i = 0; i < numBones; ++i) {
                        dst[i] = a[i];
                        AnimPose::BlendTransformAdditive(b[i], dst[i], weight);
                    }
                } else {
                    for (uint32_t i = 0; i < numBones; ++i) {
                        dst[i] = a[i];
                        AnimPose::BlendTransform(b[i], dst[i], weight);
                    }
                }
                break;
            }
            case AnimOp::Layer: {
                if (op.inputA >= numSlots) {
                    break;
                }

                const Transform* src = pool + static_cast<size_t>(op.inputA) * numBones;
                const float weight = EffectiveWeight(op, state);
                for (uint32_t i = 0; i < numBones; ++i) {
                    dst[i] = bindPose[i];
                    AnimPose::BlendTransform(src[i], dst[i], weight);
                }
                break;
            }
            case AnimOp::StateSelect: {
                uint32_t sel = (state.activeState == ANIM_INVALID_HANDLE) ? 0u : state.activeState;
                const uint32_t index = op.dataIndex + sel;
                if (index >= stateSlots.size()) {
                    break;
                }
                std::memcpy(dst, pool + static_cast<size_t>(stateSlots[index]) * numBones, boneBytes);
                break;
            }
            case AnimOp::Output: {
                if (out.transforms.size() < numBones) {
                    break;
                }
                std::memcpy(out.transforms.data(), pool + static_cast<size_t>(op.output) * numBones, boneBytes);
                break;
            }
            }
        }
    }

    std::string AnimationPlan::Dump() const
    {
        std::string out;
        out += "AnimationPlan v" + std::to_string(LayoutVersion)
             + " slots=" + std::to_string(numSlots)
             + " bones=" + std::to_string(numBones)
             + " output=" + std::to_string(outputSlot)
             + " clips=" + std::to_string(clips.size())
             + " timeSlots=" + std::to_string(numTimeSlots)
             + " weightSlots=" + std::to_string(numWeightSlots) + "\n";

        for (size_t i = 0; i < ops.size(); ++i) {
            const AnimOpRecord& op = ops[i];
            out += "  [" + std::to_string(i) + "] " + OpName(op.op)
                 + " in(" + std::to_string(op.inputA) + "," + std::to_string(op.inputB) + ")"
                 + " out=" + std::to_string(op.output)
                 + " wIdx=" + std::to_string(op.weightIndex)
                 + " tIdx=" + std::to_string(op.timeIndex)
                 + " w=" + std::to_string(op.weight)
                 + " data=" + std::to_string(op.dataIndex) + "\n";
        }
        return out;
    }

    AnimationPlanBuilder::AnimationPlanBuilder(uint32_t inNumBones, const std::vector<Transform>& inBindPose)
        : numBones(inNumBones)
        , bindPose(inBindPose)
    {
        if (bindPose.size() != numBones) {
            bindPose.assign(numBones, Transform::GetIdentity());
        }
    }

    uint32_t AnimationPlanBuilder::AllocateSlot()
    {
        return numSlots++;
    }

    uint32_t AnimationPlanBuilder::AddClip(const AnimationTrackData& data, float frameRate)
    {
        for (uint32_t i = 0; i < data.GetNumTracks(); ++i) {
            if (data.GetTrack(i).bone >= numBones) {
                AddError("clip track bone index out of range");
                break;
            }
        }

        clips.emplace_back(data);
        clipFrameRates.emplace_back(frameRate);
        return static_cast<uint32_t>(clips.size() - 1);
    }

    uint32_t AnimationPlanBuilder::AddBoneMask(const AnimationBoneMask& mask)
    {
        boneMasks.emplace_back(mask);
        return static_cast<uint32_t>(boneMasks.size() - 1);
    }

    uint32_t AnimationPlanBuilder::AddTimeSlot()
    {
        return numTimeSlots++;
    }

    uint32_t AnimationPlanBuilder::AddWeightSlot()
    {
        return numWeightSlots++;
    }

    uint32_t AnimationPlanBuilder::AddStateTable(const std::vector<uint16_t>& slots)
    {
        const uint32_t base = static_cast<uint32_t>(stateSlots.size());
        stateSlots.insert(stateSlots.end(), slots.begin(), slots.end());
        return base;
    }

    uint32_t AnimationPlanBuilder::EmitOp(const AnimOpRecord& op)
    {
        ops.emplace_back(op);
        return static_cast<uint32_t>(ops.size() - 1);
    }

    bool AnimationPlanBuilder::Validate()
    {
        if (numBones == 0) {
            AddError("plan has no bones");
        }
        if (ops.empty()) {
            AddError("plan has no operations");
        }
        if (outputSlot == ANIM_INVALID_SLOT || outputSlot >= numSlots) {
            AddError("plan output slot is invalid");
        }

        uint32_t outputCount = 0;
        std::vector<uint8_t> written(numSlots, 0);

        for (const AnimOpRecord& op : ops) {
            switch (op.op) {
            case AnimOp::ClipSample:
                if (op.dataIndex >= clips.size()) {
                    AddError("ClipSample references an unknown clip");
                }
                if (op.boneMaskIndex != ANIM_INVALID_SLOT && op.boneMaskIndex >= boneMasks.size()) {
                    AddError("ClipSample bone mask index out of range");
                }
                if (op.timeIndex != ANIM_INVALID_SLOT && op.timeIndex >= numTimeSlots) {
                    AddError("ClipSample time slot out of range");
                }
                if (op.weightIndex != ANIM_INVALID_SLOT && op.weightIndex >= numWeightSlots) {
                    AddError("ClipSample weight slot out of range");
                }
                break;
            case AnimOp::Blend:
            case AnimOp::AdditiveBlend:
                if (op.inputA >= numSlots || op.inputB >= numSlots) {
                    AddError("blend input slot out of range");
                } else if (!written[op.inputA] || !written[op.inputB]) {
                    AddError("blend input is not yet written (cycle)");
                }
                if (op.weightIndex != ANIM_INVALID_SLOT && op.weightIndex >= numWeightSlots) {
                    AddError("blend weight slot out of range");
                }
                break;
            case AnimOp::Layer:
                if (op.inputA >= numSlots) {
                    AddError("layer input slot out of range");
                } else if (!written[op.inputA]) {
                    AddError("layer input is not yet written (cycle)");
                }
                break;
            case AnimOp::StateSelect:
                if (op.dataIndex >= stateSlots.size()) {
                    AddError("StateSelect state table offset out of range");
                }
                break;
            case AnimOp::Output:
                ++outputCount;
                if (op.output >= numSlots) {
                    AddError("Output slot out of range");
                }
                break;
            }

            if (op.output < numSlots) {
                written[op.output] = 1;
            }
        }

        if (outputCount != 1) {
            AddError("plan must have exactly one Output operation");
        }

        return diagnostics.empty();
    }

    bool AnimationPlanBuilder::Build(AnimationPlan& out)
    {
        if (!Validate()) {
            return false;
        }

        out.ops = std::move(ops);
        out.clips = std::move(clips);
        out.clipFrameRates = std::move(clipFrameRates);
        out.bindPose = std::move(bindPose);
        out.stateSlots = std::move(stateSlots);
        out.boneMasks = std::move(boneMasks);
        out.numSlots = numSlots;
        out.numBones = numBones;
        out.outputSlot = outputSlot;
        out.numTimeSlots = numTimeSlots;
        out.numWeightSlots = numWeightSlots;
        out.rootBone = rootBone;
        out.hasRoot = hasRoot;
        out.posePool.assign(static_cast<size_t>(numSlots) * numBones, Transform::GetIdentity());
        return true;
    }

    namespace {

        constexpr uint32_t PLAN_MAGIC = 0x4C504E41; // 'ANPL'

        template <typename T>
        void AppendBytes(std::vector<uint8_t>& out, const T& value)
        {
            const auto* p = reinterpret_cast<const uint8_t*>(&value);
            out.insert(out.end(), p, p + sizeof(T));
        }

        template <typename T>
        void AppendArray(std::vector<uint8_t>& out, const std::vector<T>& values)
        {
            if (!values.empty()) {
                const auto* p = reinterpret_cast<const uint8_t*>(values.data());
                out.insert(out.end(), p, p + values.size() * sizeof(T));
            }
        }

        template <typename T>
        bool ReadBytes(const std::vector<uint8_t>& data, size_t& offset, T& value)
        {
            if (offset + sizeof(T) > data.size()) {
                return false;
            }
            std::memcpy(&value, data.data() + offset, sizeof(T));
            offset += sizeof(T);
            return true;
        }

        template <typename T>
        bool ReadArray(const std::vector<uint8_t>& data, size_t& offset, std::vector<T>& values, uint32_t count)
        {
            if (offset + static_cast<size_t>(count) * sizeof(T) > data.size()) {
                return false;
            }
            values.resize(count);
            if (count > 0) {
                std::memcpy(values.data(), data.data() + offset, static_cast<size_t>(count) * sizeof(T));
            }
            offset += static_cast<size_t>(count) * sizeof(T);
            return true;
        }

    } // namespace

    std::vector<uint8_t> AnimationPlanBlob::Serialize(const AnimationPlan& plan)
    {
        std::vector<uint8_t> out;
        AppendBytes(out, PLAN_MAGIC);
        AppendBytes(out, AnimationPlan::LayoutVersion);
        AppendBytes(out, plan.numSlots);
        AppendBytes(out, plan.numBones);
        AppendBytes(out, plan.outputSlot);
        AppendBytes(out, plan.numTimeSlots);
        AppendBytes(out, plan.numWeightSlots);
        AppendBytes(out, plan.rootBone);
        const uint8_t hasRoot = plan.hasRoot ? 1 : 0;
        AppendBytes(out, hasRoot);
        const uint8_t pad = 0;
        AppendBytes(out, pad);
        AppendBytes(out, static_cast<uint32_t>(plan.ops.size()));
        AppendBytes(out, static_cast<uint32_t>(plan.clips.size()));
        AppendBytes(out, static_cast<uint32_t>(plan.boneMasks.size()));

        AppendArray(out, plan.ops);
        AppendArray(out, plan.bindPose);
        AppendArray(out, plan.boneMasks);
        AppendBytes(out, static_cast<uint32_t>(plan.stateSlots.size()));
        AppendArray(out, plan.stateSlots);

        for (size_t c = 0; c < plan.clips.size(); ++c) {
            const AnimationTrackData& clip = plan.clips[c];
            const auto& tracks = clip.GetTracks();
            const auto& times = clip.GetTimes();
            const auto& translations = clip.GetTranslations();
            const auto& scales = clip.GetScales();
            const auto& rotations = clip.GetRotations();

            AppendBytes(out, static_cast<uint32_t>(tracks.size()));
            AppendBytes(out, static_cast<uint32_t>(times.size()));
            AppendBytes(out, static_cast<uint32_t>(translations.size()));
            AppendBytes(out, static_cast<uint32_t>(scales.size()));
            AppendBytes(out, static_cast<uint32_t>(rotations.size()));
            AppendBytes(out, plan.clipFrameRates[c]);

            AppendArray(out, tracks);
            AppendArray(out, times);
            AppendArray(out, translations);
            AppendArray(out, scales);
            AppendArray(out, rotations);
        }

        return out;
    }

    bool AnimationPlanBlob::Deserialize(const std::vector<uint8_t>& data, AnimationPlan& out)
    {
        size_t offset = 0;
        uint32_t magic = 0;
        uint32_t version = 0;
        uint32_t numOps = 0;
        uint32_t numClips = 0;
        uint16_t rootBone = 0;
        uint8_t hasRoot = 0;
        uint8_t pad = 0;

        if (!ReadBytes(data, offset, magic) || magic != PLAN_MAGIC) {
            return false;
        }
        if (!ReadBytes(data, offset, version) || version != AnimationPlan::LayoutVersion) {
            return false;
        }
        if (!ReadBytes(data, offset, out.numSlots)) { return false; }
        if (!ReadBytes(data, offset, out.numBones)) { return false; }
        if (!ReadBytes(data, offset, out.outputSlot)) { return false; }
        if (!ReadBytes(data, offset, out.numTimeSlots)) { return false; }
        if (!ReadBytes(data, offset, out.numWeightSlots)) { return false; }
        if (!ReadBytes(data, offset, rootBone)) { return false; }
        if (!ReadBytes(data, offset, hasRoot)) { return false; }
        if (!ReadBytes(data, offset, pad)) { return false; }
        if (!ReadBytes(data, offset, numOps)) { return false; }
        if (!ReadBytes(data, offset, numClips)) { return false; }
        uint32_t numBoneMasks = 0;
        if (!ReadBytes(data, offset, numBoneMasks)) { return false; }

        out.rootBone = rootBone;
        out.hasRoot = hasRoot != 0;

        if (!ReadArray(data, offset, out.ops, numOps)) { return false; }
        if (!ReadArray(data, offset, out.bindPose, out.numBones)) { return false; }
        if (!ReadArray(data, offset, out.boneMasks, numBoneMasks)) { return false; }

        uint32_t numStateSlots = 0;
        if (!ReadBytes(data, offset, numStateSlots)) { return false; }
        if (!ReadArray(data, offset, out.stateSlots, numStateSlots)) { return false; }

        out.clips.resize(numClips);
        out.clipFrameRates.resize(numClips);

        for (uint32_t c = 0; c < numClips; ++c) {
            uint32_t numTracks = 0;
            uint32_t numTimes = 0;
            uint32_t numTranslations = 0;
            uint32_t numScales = 0;
            uint32_t numRotations = 0;
            float frameRate = 0.f;

            if (!ReadBytes(data, offset, numTracks)) { return false; }
            if (!ReadBytes(data, offset, numTimes)) { return false; }
            if (!ReadBytes(data, offset, numTranslations)) { return false; }
            if (!ReadBytes(data, offset, numScales)) { return false; }
            if (!ReadBytes(data, offset, numRotations)) { return false; }
            if (!ReadBytes(data, offset, frameRate)) { return false; }

            AnimationTrackData& clip = out.clips[c];
            clip.Clear();
            if (!ReadArray(data, offset, clip.tracks, numTracks)) { return false; }
            if (!ReadArray(data, offset, clip.times, numTimes)) { return false; }
            if (!ReadArray(data, offset, clip.translations, numTranslations)) { return false; }
            if (!ReadArray(data, offset, clip.scales, numScales)) { return false; }
            if (!ReadArray(data, offset, clip.rotations, numRotations)) { return false; }
            out.clipFrameRates[c] = frameRate;
        }

        out.posePool.assign(static_cast<size_t>(out.numSlots) * out.numBones, Transform::GetIdentity());
        return true;
    }

} // namespace sky

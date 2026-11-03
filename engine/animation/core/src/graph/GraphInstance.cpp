#include <animation/graph/GraphInstance.h>

#include <algorithm>
#include <animation/graph/AnimationClipNode.h>
#include <animation/plan/AnimationPlan.h>
#include <array>
#include <cmath>

namespace sky::animation {
    namespace {
        constexpr uint32_t INVALID = UINT32_MAX;
        AnimationStatus    Invalid(std::string text)
        {
            return {AnimationError::InvalidAsset, std::move(text)};
        }
        bool Compare(const AnimationParam &actual, const AnimationParam &expected, GraphComparison comparison)
        {
            return std::visit(
                [&](const auto &value) {
                    using T           = std::decay_t<decltype(value)>;
                    const auto target = std::get<T>(expected);
                    switch (comparison) {
                    case GraphComparison::Equal: return value == target;
                    case GraphComparison::NotEqual: return value != target;
                    case GraphComparison::Less: return value < target;
                    case GraphComparison::LessEqual: return value <= target;
                    case GraphComparison::Greater: return value > target;
                    case GraphComparison::GreaterEqual: return value >= target;
                    }
                    return false;
                },
                actual);
        }
    } // namespace
    struct GraphInstance::Impl {
        struct Clock {
            Uuid                               id;
            std::unique_ptr<AnimationClipNode> node;
            double                             elapsed  = 0.0;
            float                              duration = 0.f;
            float                              rate     = 1.f;
            bool                               loop     = true;
            uint32_t                           timeSlot = 0;
        };
        struct Blend {
            uint32_t slot      = 0;
            uint32_t parameter = INVALID;
            float    value     = 0.f;
        };
        struct State {
            Uuid               id;
            AnimationPlan      plan;
            AnimFrameState     frame;
            AnimPose           pose;
            std::vector<Clock> clocks;
            std::vector<Blend> blends;
        };
        struct Condition {
            uint32_t        parameter = 0;
            AnimationParam  value;
            GraphComparison comparison = GraphComparison::Equal;
        };
        struct Transition {
            Uuid                   id;
            uint32_t               from      = 0;
            uint32_t               to        = 0;
            uint32_t               clock     = INVALID;
            float                  threshold = 0.f;
            float                  duration  = 0.f;
            std::vector<Condition> conditions;
        };
        SkeletonPtr                        skeleton;
        std::vector<State>                 states;
        std::vector<Transition>            transitions;
        std::unordered_map<Uuid, uint32_t> parameterIndices;
        std::vector<AnimationParam>        parameters;
        std::vector<AnimationParam>        snapshot;
        uint32_t                           entry    = 0;
        uint32_t                           active   = 0;
        uint32_t                           fading   = INVALID;
        double                             fadeTime = 0.0;
        float                              rate     = 1.f;
        bool                               playing  = false;

        void ResetState(uint32_t index)
        {
            auto &state = states[index];
            for (auto &clock : state.clocks) {
                clock.elapsed = 0.0;
                clock.node->Stop();
                state.frame.clipTimes[clock.timeSlot] = 0.f;
            }
        }
        void Advance(uint32_t index, double delta)
        {
            for (auto &clock : states[index].clocks) {
                // Exit clocks never wrap. Sample clocks wrap independently for looping clips.
                clock.elapsed                                 = std::min(clock.elapsed + delta * clock.rate, std::numeric_limits<double>::max());
                const double time                             = clock.duration <= 0.f ? 0.0
                                                                                      : (clock.loop ? std::fmod(clock.elapsed, static_cast<double>(clock.duration))
                                                                                                    : std::min(clock.elapsed, static_cast<double>(clock.duration)));
                states[index].frame.clipTimes[clock.timeSlot] = static_cast<float>(time);
            }
        }
        bool Matches(const Transition &transition) const
        {
            if (transition.from != active) {
                return false;
            }
            if (transition.clock != INVALID) {
                const auto &clock = states[active].clocks[transition.clock];
                if (clock.duration > 0.f && clock.elapsed / clock.duration < transition.threshold) {
                    return false;
                }
            }
            for (const auto &condition : transition.conditions) {
                if (!Compare(snapshot[condition.parameter], condition.value, condition.comparison)) {
                    return false;
                }
            }
            return true;
        }
        void Sample(uint32_t index)
        {
            auto &state = states[index];
            for (const auto &blend : state.blends) {
                const float alpha               = blend.parameter == INVALID ? blend.value : std::get<float>(snapshot[blend.parameter]);
                state.frame.weights[blend.slot] = std::clamp(alpha, 0.f, 1.f);
            }
            state.plan.Evaluate(state.frame, state.pose);
        }
    };

    GraphInstance::GraphInstance() : impl(std::make_unique<Impl>())
    {
    }
    GraphInstance::~GraphInstance() = default;

    std::unique_ptr<GraphInstance>
    GraphInstance::Create(const CookedGraph &input, const SkeletonPtr &skeleton, const Clips &clips, AnimationStatus &status)
    {
        status = {};
        if (!skeleton) {
            status = Invalid("Graph instance requires a skeleton");
            return {};
        }
        CookedGraph cooked;
        const auto  checked = CompileGraphDefinition(input.definition, cooked);
        if (!checked) {
            status = Invalid(checked.diagnostics.front().message);
            return {};
        }
        const auto &graph = cooked.definition;
        for (const auto &node : graph.nodes) {
            if (node.kind != GraphNodeKind::Clip) {
                continue;
            }
            const auto found = clips.find(node.clip);
            if (found == clips.end() || !found->second) {
                status = Invalid("Graph Clip resource is missing: " + node.id.ToString());
                return {};
            }
            status = found->second->Validate(skeleton.Get());
            if (!status) {
                return {};
            }
        }
        auto  result     = std::unique_ptr<GraphInstance>(new GraphInstance());
        auto &runtime    = *result->impl;
        runtime.skeleton = skeleton;
        std::unordered_map<Uuid, uint32_t> nodeIndices, stateIndices;
        for (uint32_t i = 0; i < graph.nodes.size(); ++i) {
            nodeIndices.emplace(graph.nodes[i].id, i);
        }
        for (uint32_t i = 0; i < graph.parameters.size(); ++i) {
            runtime.parameterIndices.emplace(graph.parameters[i].id, i);
            runtime.parameters.push_back(graph.parameters[i].value);
        }
        runtime.snapshot = runtime.parameters;
        std::vector<std::array<uint32_t, 2>> inputs(graph.nodes.size(), {INVALID, INVALID});
        for (const auto &link : graph.links) {
            inputs[nodeIndices.at(link.to)][link.input == GraphPin::InputA ? 0 : 1] = nodeIndices.at(link.from);
        }
        std::vector<Uuid> roots;
        if (graph.states.empty()) {
            roots.push_back(graph.output);
        } else {
            for (const auto &state : graph.states) {
                roots.push_back(state.output);
            }
        }
        runtime.states.resize(roots.size());
        std::vector<uint32_t> owners(graph.nodes.size(), INVALID);
        for (uint32_t i = 0; i < roots.size(); ++i) {
            std::vector<uint32_t> pending{nodeIndices.at(roots[i])};
            while (!pending.empty()) {
                const auto node = pending.back();
                pending.pop_back();
                owners[node] = i;
                for (const auto child : inputs[node]) {
                    if (child != INVALID) {
                        pending.push_back(child);
                    }
                }
            }
            if (!graph.states.empty()) {
                runtime.states[i].id = graph.states[i].id;
                stateIndices.emplace(graph.states[i].id, i);
            }
        }
        std::unordered_map<Uuid, uint32_t> clockIndices;
        for (uint32_t stateIndex = 0; stateIndex < runtime.states.size(); ++stateIndex) {
            auto                 &state = runtime.states[stateIndex];
            AnimationPlanBuilder  builder(static_cast<uint32_t>(skeleton->GetNumBones()), skeleton->GetRefPos()->transforms);
            std::vector<uint32_t> slots(graph.nodes.size(), INVALID);
            for (const auto index : cooked.topologicalNodes) {
                if (owners[index] != stateIndex) {
                    continue;
                }
                const auto &node = graph.nodes[index];
                if (node.kind == GraphNodeKind::Clip) {
                    Impl::Clock clock;
                    clock.id       = node.id;
                    clock.rate     = node.rate;
                    clock.loop     = node.loop;
                    clock.duration = clips.at(node.clip)->GetDuration();
                    clock.node     = std::make_unique<AnimationClipNode>(AnimationClipNode::PersistentData{clips.at(node.clip), node.loop, true});
                    clock.node->InitControl();
                    if (!clock.node->GetStatus()) {
                        status = clock.node->GetStatus();
                        return {};
                    }
                    if (!clock.node->LowerToPlan(builder, {skeleton.Get()}, slots[index])) {
                        status = Invalid("Graph Clip lowering failed: " + node.id.ToString());
                        return {};
                    }
                    clock.timeSlot = builder.GetClipBindings().back().timeSlot;
                    clockIndices.emplace(node.id, static_cast<uint32_t>(state.clocks.size()));
                    state.clocks.push_back(std::move(clock));
                } else if (node.kind == GraphNodeKind::Blend2) {
                    const auto  weight = builder.AddWeightSlot();
                    Impl::Blend blend;
                    blend.slot  = weight;
                    blend.value = node.alpha;
                    if (node.alphaParameter) {
                        blend.parameter = runtime.parameterIndices.at(node.alphaParameter);
                    }
                    state.blends.push_back(blend);
                    slots[index] = builder.AllocateSlot();
                    AnimOpRecord op;
                    op.op          = AnimOp::Blend;
                    op.inputA      = static_cast<uint16_t>(slots[inputs[index][0]]);
                    op.inputB      = static_cast<uint16_t>(slots[inputs[index][1]]);
                    op.output      = static_cast<uint16_t>(slots[index]);
                    op.weightIndex = static_cast<uint16_t>(weight);
                    builder.EmitOp(op);
                } else {
                    slots[index] = slots[inputs[index][0]];
                    builder.SetOutputSlot(slots[index]);
                    AnimOpRecord op;
                    op.op     = AnimOp::Output;
                    op.output = static_cast<uint16_t>(slots[index]);
                    builder.EmitOp(op);
                }
            }
            if (!builder.Build(state.plan)) {
                status = Invalid("Graph Plan validation failed");
                return {};
            }
            state.frame.Resize(state.plan.GetNumTimeSlots(), state.plan.GetNumWeightSlots(), state.plan.GetNumOps());
            state.pose.skeleton = skeleton.Get();
            state.pose.ResetRefPose();
        }
        runtime.entry  = graph.states.empty() ? 0u : stateIndices.at(graph.entryState);
        runtime.active = runtime.entry;
        for (const auto &source : graph.transitions) {
            Impl::Transition transition;
            transition.id       = source.id;
            transition.from     = stateIndices.at(source.from);
            transition.to       = stateIndices.at(source.to);
            transition.duration = source.duration;
            if (source.hasExitTime) {
                transition.clock     = clockIndices.at(source.timeSourceNode);
                transition.threshold = source.exitTime;
                if (runtime.states[transition.from].clocks[transition.clock].duration == 0.f && transition.threshold > 0.f) {
                    status = Invalid("Zero-duration exit clocks only support threshold zero");
                    return {};
                }
            }
            for (const auto &condition : source.conditions) {
                transition.conditions.push_back({runtime.parameterIndices.at(condition.parameter), condition.value, condition.comparison});
            }
            runtime.transitions.push_back(std::move(transition));
        }
        return result;
    }
    AnimationStatus GraphInstance::Tick(float delta)
    {
        if (!std::isfinite(delta) || delta < 0.f) {
            return {AnimationError::InvalidPlayback, "Graph delta must be finite and nonnegative"};
        }
        auto &runtime    = *impl;
        runtime.snapshot = runtime.parameters;
        if (!runtime.playing || runtime.rate == 0.f) {
            return {};
        }
        if (runtime.fading == INVALID) {
            for (uint32_t i = 0; i < runtime.transitions.size(); ++i) {
                const auto &transition = runtime.transitions[i];
                if (!runtime.Matches(transition)) {
                    continue;
                }
                runtime.ResetState(transition.to);
                if (transition.duration == 0.f) {
                    runtime.active = transition.to;
                } else {
                    runtime.fading   = i;
                    runtime.fadeTime = 0.0;
                }
                break;
            }
        }
        const double step = static_cast<double>(delta) * runtime.rate;
        if (runtime.fading == INVALID) {
            runtime.Advance(runtime.active, step);
        } else {
            const auto &transition = runtime.transitions[runtime.fading];
            runtime.Advance(transition.from, std::min(step, std::max(0.0, transition.duration - runtime.fadeTime)));
            runtime.Advance(transition.to, step);
            runtime.fadeTime += step;
            if (runtime.fadeTime >= transition.duration) {
                runtime.active = transition.to;
                runtime.fading = INVALID;
            }
        }
        return {};
    }
    void GraphInstance::Evaluate(AnimPose &out)
    {
        auto &runtime    = *impl;
        runtime.snapshot = runtime.parameters;
        runtime.Sample(runtime.active);
        out = runtime.states[runtime.active].pose;
        if (runtime.fading != INVALID) {
            const auto &transition = runtime.transitions[runtime.fading];
            runtime.Sample(transition.to);
            AnimPose::BlendPose(runtime.states[transition.to].pose, out, static_cast<float>(runtime.fadeTime / transition.duration),
                                PoseBlendMode::OVERRIDE);
        }
    }
    AnimationStatus GraphInstance::SetParameter(const Uuid &id, const AnimationParam &value)
    {
        const auto found = impl->parameterIndices.find(id);
        if (found == impl->parameterIndices.end() || impl->parameters[found->second].index() != value.index() ||
            (std::holds_alternative<float>(value) && !std::isfinite(std::get<float>(value)))) {
            return {AnimationError::InvalidPlayback, "Graph parameter ID, type or value is invalid"};
        }
        impl->parameters[found->second] = value;
        return {};
    }
    const std::vector<AnimationParam> &GraphInstance::GetParameters() const
    {
        return impl->parameters;
    }
    AnimationStatus GraphInstance::SetRate(float value)
    {
        if (!std::isfinite(value) || value < 0.f) {
            return {AnimationError::InvalidPlayback, "Graph rate must be finite and nonnegative"};
        }
        impl->rate = value;
        return {};
    }
    void GraphInstance::SetPlaying(bool value)
    {
        impl->playing = value;
    }
    bool GraphInstance::IsPlaying() const
    {
        return impl->playing;
    }
    void GraphInstance::Stop()
    {
        impl->playing  = false;
        impl->active   = impl->entry;
        impl->fading   = INVALID;
        impl->fadeTime = 0.0;
        for (uint32_t i = 0; i < impl->states.size(); ++i) {
            impl->ResetState(i);
        }
    }
    GraphDebugState GraphInstance::Debug() const
    {
        GraphDebugState result;
        result.activeState   = impl->states[impl->active].id;
        result.playing       = impl->playing;
        uint32_t destination = INVALID;
        if (impl->fading != INVALID) {
            const auto &transition  = impl->transitions[impl->fading];
            destination             = transition.to;
            result.destinationState = impl->states[destination].id;
            result.transition       = transition.id;
            result.progress         = static_cast<float>(impl->fadeTime / transition.duration);
        }
        for (uint32_t i = 0; i < impl->states.size(); ++i) {
            for (const auto &clock : impl->states[i].clocks) {
                result.nodes.push_back(
                    {clock.id, impl->states[i].frame.clipTimes[clock.timeSlot], clock.elapsed, i == impl->active || i == destination});
            }
        }
        return result;
    }
} // namespace sky::animation

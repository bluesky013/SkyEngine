#include "animation/editor/GraphProperties.h"

#include <framework/serialization/PropertyCommon.h>
#include <framework/serialization/SerializationContext.h>

namespace sky::animation::editor {
    void RegisterGraphProperties()
    {
        auto *context = SerializationContext::Get();
        context->Register<GraphParameterType>("GraphParameterType").Enum(GraphParameterType::Bool, "Bool").Enum(GraphParameterType::Float, "Float");
        context->Register<GraphComparison>("GraphComparison")
            .Enum(GraphComparison::Equal, "Equal")
            .Enum(GraphComparison::NotEqual, "NotEqual")
            .Enum(GraphComparison::Less, "Less")
            .Enum(GraphComparison::LessEqual, "LessEqual")
            .Enum(GraphComparison::Greater, "Greater")
            .Enum(GraphComparison::GreaterEqual, "GreaterEqual");
        context->Register<ParameterProperties>("GraphParameterDefaults")
            .Member<&ParameterProperties::id>("id")
            .Property(CommonPropertyKey::ASSET_TYPE, Any(std::string("GraphParameter")))
            .Property(CommonPropertyKey::READONLY, Any(true))
            .Member<&ParameterProperties::name>("name")
            .Member<&ParameterProperties::type>("type")
            .Member<&ParameterProperties::booleanDefault>("booleanDefault")
            .Member<&ParameterProperties::floatDefault>("floatDefault");
        context->Register<ConditionProperties>("GraphConditionSettings")
            .Member<&ConditionProperties::parameter>("parameter")
            .Property(CommonPropertyKey::ASSET_TYPE, Any(std::string("GraphParameter")))
            .Member<&ConditionProperties::comparison>("comparison")
            .Member<&ConditionProperties::type>("type")
            .Member<&ConditionProperties::booleanValue>("booleanValue")
            .Member<&ConditionProperties::floatValue>("floatValue");
        context->Register<StateProperties>("GraphStateSettings")
            .Member<&StateProperties::id>("id")
            .Property(CommonPropertyKey::ASSET_TYPE, Any(std::string("GraphState")))
            .Property(CommonPropertyKey::READONLY, Any(true))
            .Member<&StateProperties::name>("name")
            .Member<&StateProperties::output>("output")
            .Property(CommonPropertyKey::ASSET_TYPE, Any(std::string("GraphNode")));
        context->Register<TransitionProperties>("GraphTransitionSettings")
            .Member<&TransitionProperties::id>("id")
            .Property(CommonPropertyKey::ASSET_TYPE, Any(std::string("GraphTransition")))
            .Property(CommonPropertyKey::READONLY, Any(true))
            .Member<&TransitionProperties::from>("from")
            .Property(CommonPropertyKey::ASSET_TYPE, Any(std::string("GraphState")))
            .Member<&TransitionProperties::to>("to")
            .Property(CommonPropertyKey::ASSET_TYPE, Any(std::string("GraphState")))
            .Member<&TransitionProperties::conditions>("conditions")
            .Member<&TransitionProperties::priority>("priority")
            .Member<&TransitionProperties::duration>("duration")
            .Member<&TransitionProperties::hasExitTime>("hasExitTime")
            .Member<&TransitionProperties::timeSourceNode>("timeSourceNode")
            .Property(CommonPropertyKey::ASSET_TYPE, Any(std::string("GraphNode")))
            .Member<&TransitionProperties::exitTime>("exitTime")
            .Property(CommonPropertyKey::RANGE_MIN, Any(0.f))
            .Property(CommonPropertyKey::RANGE_MAX, Any(1.f));
        context->Register<GraphProperties>("GraphSettings")
            .Member<&GraphProperties::skeleton>("skeleton")
            .Property(CommonPropertyKey::ASSET_TYPE, Any(std::string("AuroraSkeleton")))
            .Member<&GraphProperties::output>("output")
            .Property(CommonPropertyKey::ASSET_TYPE, Any(std::string("GraphNode")))
            .Member<&GraphProperties::entryState>("entryState")
            .Property(CommonPropertyKey::ASSET_TYPE, Any(std::string("GraphState")))
            .Member<&GraphProperties::parameters>("parameters")
            .Member<&GraphProperties::states>("states")
            .Member<&GraphProperties::transitions>("transitions");
        context->Register<ClipNodeProperties>("GraphClipNodeSettings")
            .Member<&ClipNodeProperties::clip>("clip")
            .Property(CommonPropertyKey::ASSET_TYPE, Any(std::string("AnimationClip")))
            .Member<&ClipNodeProperties::loop>("loop")
            .Member<&ClipNodeProperties::rate>("rate");
        context->Register<BlendNodeProperties>("GraphBlendNodeSettings")
            .Member<&BlendNodeProperties::alpha>("alpha")
            .Property(CommonPropertyKey::RANGE_MIN, Any(0.f))
            .Property(CommonPropertyKey::RANGE_MAX, Any(1.f))
            .Member<&BlendNodeProperties::alphaParameter>("alphaParameter")
            .Property(CommonPropertyKey::ASSET_TYPE, Any(std::string("GraphParameter")));
    }
    GraphProperties ReadGraphProperties(const GraphDefinition &graph)
    {
        GraphProperties result{graph.skeleton, graph.output, graph.entryState};
        for (const auto &p : graph.parameters)
            result.parameters.push_back({p.id, p.name, p.type, std::holds_alternative<bool>(p.value) ? std::get<bool>(p.value) : false,
                                         std::holds_alternative<float>(p.value) ? std::get<float>(p.value) : 0.f});
        for (const auto &s : graph.states)
            result.states.push_back({s.id, s.name, s.output});
        for (const auto &t : graph.transitions) {
            TransitionProperties next{t.id, t.from, t.to, {}, t.priority, t.duration, t.hasExitTime, t.timeSourceNode, t.exitTime};
            for (const auto &c : t.conditions) {
                const bool boolean = std::holds_alternative<bool>(c.value);
                next.conditions.push_back({c.parameter, c.comparison, boolean ? GraphParameterType::Bool : GraphParameterType::Float,
                                           boolean ? std::get<bool>(c.value) : false, boolean ? 0.f : std::get<float>(c.value)});
            }
            result.transitions.push_back(std::move(next));
        }
        return result;
    }
    void WriteGraphProperties(const GraphProperties &properties, GraphDefinition &graph)
    {
        graph.skeleton   = properties.skeleton;
        graph.output     = properties.output;
        graph.entryState = properties.entryState;
        graph.parameters.clear();
        graph.states.clear();
        graph.transitions.clear();
        for (const auto &p : properties.parameters)
            graph.parameters.push_back(
                {p.id, p.name, p.type, p.type == GraphParameterType::Bool ? AnimationParam(p.booleanDefault) : AnimationParam(p.floatDefault)});
        for (const auto &s : properties.states)
            graph.states.push_back({s.id, s.name, s.output});
        for (const auto &t : properties.transitions) {
            GraphTransition next{t.id, t.from, t.to, {}, t.priority, t.duration, t.hasExitTime, t.timeSourceNode, t.exitTime};
            for (const auto &c : t.conditions)
                next.conditions.push_back(
                    {c.parameter, c.comparison, c.type == GraphParameterType::Bool ? AnimationParam(c.booleanValue) : AnimationParam(c.floatValue)});
            graph.transitions.push_back(std::move(next));
        }
    }
} // namespace sky::animation::editor

#pragma once

#include <animation/graph/GraphDefinition.h>

namespace sky::animation::editor {
    struct ParameterProperties {
        Uuid               id             = Uuid::Create();
        std::string        name           = "Parameter";
        GraphParameterType type           = GraphParameterType::Float;
        bool               booleanDefault = false;
        float              floatDefault   = 0.f;
    };
    struct ConditionProperties {
        Uuid               parameter;
        GraphComparison    comparison   = GraphComparison::Equal;
        GraphParameterType type         = GraphParameterType::Bool;
        bool               booleanValue = false;
        float              floatValue   = 0.f;
    };
    struct StateProperties {
        Uuid        id   = Uuid::Create();
        std::string name = "State";
        Uuid        output;
    };
    struct TransitionProperties {
        Uuid                             id = Uuid::Create();
        Uuid                             from, to;
        std::vector<ConditionProperties> conditions;
        int32_t                          priority    = 0;
        float                            duration    = 0.f;
        bool                             hasExitTime = false;
        Uuid                             timeSourceNode;
        float                            exitTime = 0.f;
    };
    struct GraphProperties {
        Uuid                              skeleton, output, entryState;
        std::vector<ParameterProperties>  parameters;
        std::vector<StateProperties>      states;
        std::vector<TransitionProperties> transitions;
    };
    struct ClipNodeProperties {
        Uuid  clip;
        bool  loop = true;
        float rate = 1.f;
    };
    struct BlendNodeProperties {
        float alpha = .5f;
        Uuid  alphaParameter;
    };
    void            RegisterGraphProperties();
    GraphProperties ReadGraphProperties(const GraphDefinition &graph);
    void            WriteGraphProperties(const GraphProperties &properties, GraphDefinition &graph);
} // namespace sky::animation::editor

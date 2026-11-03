#pragma once

#include <animation/core/AnimationTypes.h>
#include <core/util/Uuid.h>
#include <string>

namespace sky::animation {
    inline constexpr uint32_t GRAPH_VERSION = 2;
    enum class GraphNodeKind : uint8_t { Clip, Blend2, Output };
    enum class GraphPin : uint8_t { Pose, InputA, InputB };
    enum class GraphParameterType : uint8_t { Bool, Float };
    enum class GraphComparison : uint8_t { Equal, NotEqual, Less, LessEqual, Greater, GreaterEqual };
    enum class GraphValidationMode : uint8_t { Draft, Executable };

    struct GraphParameter {
        Uuid               id;
        std::string        name;
        GraphParameterType type  = GraphParameterType::Float;
        AnimationParam     value = 0.f;
    };
    struct GraphNode {
        Uuid          id;
        GraphNodeKind kind = GraphNodeKind::Clip;
        Uuid          clip;
        bool          loop  = true;
        float         rate  = 1.f;
        float         alpha = 0.5f;
        Uuid          alphaParameter;
    };
    struct GraphLink {
        Uuid     from;
        GraphPin output = GraphPin::Pose;
        Uuid     to;
        GraphPin input = GraphPin::InputA;
    };
    struct GraphState {
        Uuid        id;
        std::string name;
        Uuid        output;
    };
    struct GraphCondition {
        Uuid            parameter;
        GraphComparison comparison = GraphComparison::Equal;
        AnimationParam  value      = false;
    };
    struct GraphTransition {
        Uuid                        id;
        Uuid                        from;
        Uuid                        to;
        std::vector<GraphCondition> conditions;
        int32_t                     priority    = 0;
        float                       duration    = 0.f;
        bool                        hasExitTime = false;
        Uuid                        timeSourceNode;
        float                       exitTime = 0.f;
    };
    struct GraphDefinition {
        uint32_t                     version = GRAPH_VERSION;
        Uuid                         document;
        Uuid                         skeleton;
        Uuid                         output;
        Uuid                         entryState;
        std::vector<GraphParameter>  parameters;
        std::vector<GraphNode>       nodes;
        std::vector<GraphLink>       links;
        std::vector<GraphState>      states;
        std::vector<GraphTransition> transitions;
    };
    struct GraphNodePosition {
        Uuid  node;
        float x = 0.f;
        float y = 0.f;
    };
    struct GraphEditorLayout {
        std::vector<GraphNodePosition> nodes;
        float                          panX = 0.f;
        float                          panY = 0.f;
        float                          zoom = 1.f;
    };
    struct GraphSource {
        GraphDefinition   definition;
        GraphEditorLayout editor;
    };
    struct GraphDiagnostic {
        Uuid        object;
        std::string pin;
        std::string message;
    };
    struct GraphValidation {
        std::vector<GraphDiagnostic> diagnostics;
        explicit                     operator bool() const
        {
            return diagnostics.empty();
        }
    };
    struct CookedGraph {
        GraphDefinition       definition;
        std::vector<uint32_t> topologicalNodes;
        std::vector<Uuid>     dependencies;
    };
    GraphValidation ValidateGraph(const GraphDefinition &graph, GraphValidationMode mode = GraphValidationMode::Executable);
    GraphValidation ValidateGraphSource(const GraphSource &source, GraphValidationMode mode = GraphValidationMode::Draft);
    GraphValidation CompileGraphDefinition(const GraphDefinition &graph, CookedGraph &out);
} // namespace sky::animation

//
// Created by blues on 2026/3/10.
//

#pragma once

#include <shader/shadergraph/ShaderGraphNode.h>
#include <shader/shadergraph/ShaderGraphPin.h>
#include <unordered_map>
#include <vector>
#include <string>
#include <functional>

namespace sky::sg {

    // The shader graph – owns all nodes and the connections between them.
    class ShaderGraph {
    public:
        ShaderGraph() = default;
        ~ShaderGraph() = default;

        // Node management
        void         AddNode(const SGNodePtr& node);
        void         RemoveNode(const Uuid& nodeId);
        SGNodePtr    FindNode(const Uuid& nodeId) const;
        const std::unordered_map<Uuid, SGNodePtr>& GetNodes() const { return nodes; }

        // Connection management
        bool AddConnection(const SGConnection& conn);
        void RemoveConnection(const SGConnection& conn);
        const std::vector<SGConnection>& GetConnections() const { return connections; }

        // Find which output pin feeds a given input pin (returns invalid SGPinID if unconnected)
        SGPinID GetSourcePin(const SGPinID& inputPin) const;

        // HLSL code generation – returns the full shader function body and declarations
        struct GeneratedCode {
            std::string declarations; // textures, samplers, cbuffer params
            std::string body;         // the SurfaceShader() function body
        };
        GeneratedCode GenerateHLSL() const;

        // Serialization
        void LoadJson(JsonInputArchive& archive);
        void SaveJson(JsonOutputArchive& archive) const;

        // Node registry: maps type name → factory
        using FactoryFn = std::function<SGNodePtr()>;
        static void RegisterNodeType(const std::string& typeName, FactoryFn factory);
        static SGNodePtr CreateNodeByType(const std::string& typeName);

    private:
        // Topological traversal helper
        void CollectInputs(const SGNodePtr& node,
                           std::vector<std::string>& inputVars,
                           SGCodeGenContext& ctx,
                           std::unordered_map<Uuid, std::vector<std::string>>& cache) const;

        std::unordered_map<Uuid, SGNodePtr> nodes;
        std::vector<SGConnection>                      connections;

        static std::unordered_map<std::string, FactoryFn>& GetRegistry();
    };

} // namespace sky::sg

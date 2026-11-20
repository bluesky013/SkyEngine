//
// Created by blues on 2026/3/10.
//

#include <shader/shadergraph/ShaderGraph.h>
#include <shader/shadergraph/ShaderGraphMathNodes.h>
#include <shader/shadergraph/ShaderGraphInputNodes.h>
#include <shader/shadergraph/ShaderGraphOutputNode.h>
#include <framework/serialization/JsonArchive.h>
#include <algorithm>

namespace sky::sg {

    // ---- Node registry ----

    std::unordered_map<std::string, ShaderGraph::FactoryFn>& ShaderGraph::GetRegistry()
    {
        static std::unordered_map<std::string, FactoryFn> registry;
        return registry;
    }

    void ShaderGraph::RegisterNodeType(const std::string& typeName, FactoryFn factory)
    {
        GetRegistry()[typeName] = std::move(factory);
    }

    SGNodePtr ShaderGraph::CreateNodeByType(const std::string& typeName)
    {
        auto& reg = GetRegistry();
        auto it = reg.find(typeName);
        if (it != reg.end()) {
            return it->second();
        }
        return nullptr;
    }

    // ---- Node management ----

    void ShaderGraph::AddNode(const SGNodePtr& node)
    {
        if (node) {
            nodes[node->GetId()] = node;
        }
    }

    void ShaderGraph::RemoveNode(const Uuid& nodeId)
    {
        nodes.erase(nodeId);
        // Remove all connections touching this node
        connections.erase(
            std::remove_if(connections.begin(), connections.end(),
                [&nodeId](const SGConnection& c) {
                    return c.src.nodeId == nodeId || c.dst.nodeId == nodeId;
                }),
            connections.end());
    }

    SGNodePtr ShaderGraph::FindNode(const Uuid& nodeId) const
    {
        auto it = nodes.find(nodeId);
        return it != nodes.end() ? it->second : nullptr;
    }

    // ---- Connection management ----

    bool ShaderGraph::AddConnection(const SGConnection& conn)
    {
        if (!conn.IsValid()) {
            return false;
        }

        // Validate nodes and pins exist
        auto srcNode = FindNode(conn.src.nodeId);
        auto dstNode = FindNode(conn.dst.nodeId);
        if (!srcNode || !dstNode) {
            return false;
        }
        if (conn.src.pinIndex >= srcNode->GetOutputPins().size()) {
            return false;
        }
        if (conn.dst.pinIndex >= dstNode->GetInputPins().size()) {
            return false;
        }

        // Each input pin may only have one incoming connection – remove any existing
        connections.erase(
            std::remove_if(connections.begin(), connections.end(),
                [&conn](const SGConnection& c) {
                    return c.dst == conn.dst;
                }),
            connections.end());

        connections.push_back(conn);
        return true;
    }

    void ShaderGraph::RemoveConnection(const SGConnection& conn)
    {
        connections.erase(
            std::remove_if(connections.begin(), connections.end(),
                [&conn](const SGConnection& c) {
                    return c.src == conn.src && c.dst == conn.dst;
                }),
            connections.end());
    }

    SGPinID ShaderGraph::GetSourcePin(const SGPinID& inputPin) const
    {
        for (const auto& conn : connections) {
            if (conn.dst == inputPin) {
                return conn.src;
            }
        }
        return {};
    }

    // ---- HLSL code generation ----

    void ShaderGraph::CollectInputs(const SGNodePtr& node,
                                     std::vector<std::string>& inputVars,
                                     SGCodeGenContext& ctx,
                                     std::unordered_map<Uuid, std::vector<std::string>>& cache) const
    {
        const auto& inPins = node->GetInputPins();
        inputVars.resize(inPins.size());

        for (size_t i = 0; i < inPins.size(); ++i) {
            SGPinID inputPin{node->GetId(), static_cast<uint32_t>(i)};
            SGPinID srcPin = GetSourcePin(inputPin);

            if (!srcPin.IsValid()) {
                inputVars[i] = ""; // unconnected – node will use default
                continue;
            }

            auto srcNode = FindNode(srcPin.nodeId);
            if (!srcNode) {
                inputVars[i] = "";
                continue;
            }

            // Check cache
            auto cacheIt = cache.find(srcPin.nodeId);
            if (cacheIt != cache.end()) {
                const auto& outs = cacheIt->second;
                inputVars[i] = srcPin.pinIndex < outs.size() ? outs[srcPin.pinIndex] : "";
                continue;
            }

            // Recursively generate source node
            std::vector<std::string> srcInputs;
            CollectInputs(srcNode, srcInputs, ctx, cache);

            std::vector<std::string> srcOutputs;
            srcNode->GenerateHLSL(srcInputs, srcOutputs, ctx);
            cache[srcPin.nodeId] = srcOutputs;

            inputVars[i] = srcPin.pinIndex < srcOutputs.size() ? srcOutputs[srcPin.pinIndex] : "";
        }
    }

    ShaderGraph::GeneratedCode ShaderGraph::GenerateHLSL() const
    {
        SGCodeGenContext ctx;

        // Find the material output node
        SGNodePtr outputNode;
        for (const auto& [id, node] : nodes) {
            if (node->GetTypeName() == "MaterialOutput") {
                outputNode = node;
                break;
            }
        }

        if (!outputNode) {
            return {};
        }

        std::unordered_map<Uuid, std::vector<std::string>> cache;
        std::vector<std::string> inputVars;
        CollectInputs(outputNode, inputVars, ctx, cache);

        std::vector<std::string> outputVars;
        outputNode->GenerateHLSL(inputVars, outputVars, ctx);

        return {ctx.declarations, ctx.bodyCode};
    }

    // ---- Serialization ----

    void ShaderGraph::SaveJson(JsonOutputArchive& archive) const
    {
        archive.StartObject();

        // Save nodes
        archive.Key("nodes");
        archive.StartArray();
        for (const auto& [id, node] : nodes) {
            archive.StartObject();
            node->SaveJson(archive);
            archive.EndObject();
        }
        archive.EndArray();

        // Save connections
        archive.Key("connections");
        archive.StartArray();
        for (const auto& conn : connections) {
            archive.StartObject();
            archive.SaveValueObject("srcNode", conn.src.nodeId);
            archive.Key("srcPin"); archive.SaveValue(conn.src.pinIndex);
            archive.SaveValueObject("dstNode", conn.dst.nodeId);
            archive.Key("dstPin"); archive.SaveValue(conn.dst.pinIndex);
            archive.EndObject();
        }
        archive.EndArray();

        archive.EndObject();
    }

    void ShaderGraph::LoadJson(JsonInputArchive& archive)
    {
        nodes.clear();
        connections.clear();

        uint32_t nodeCount = archive.StartArray("nodes");
        for (uint32_t i = 0; i < nodeCount; ++i) {
            std::string typeName;
            archive.LoadKeyValue("type", typeName);

            auto node = CreateNodeByType(typeName);
            if (node) {
                node->LoadJson(archive);
                nodes[node->GetId()] = node;
            }
            archive.NextArrayElement();
        }
        archive.End();

        uint32_t connCount = archive.StartArray("connections");
        for (uint32_t i = 0; i < connCount; ++i) {
            SGConnection conn;
            archive.LoadKeyValue("srcNode", conn.src.nodeId);
            archive.LoadKeyValue("srcPin",  conn.src.pinIndex);
            archive.LoadKeyValue("dstNode", conn.dst.nodeId);
            archive.LoadKeyValue("dstPin",  conn.dst.pinIndex);
            connections.push_back(conn);
            archive.NextArrayElement();
        }
        archive.End();
    }

    // ---- Default node type registration ----

    namespace {
        struct SGNodeTypeRegistrar {
            SGNodeTypeRegistrar()
            {
                // Math nodes
                ShaderGraph::RegisterNodeType("Add",            []() -> SGNodePtr { return std::make_shared<SGAddNode>(); });
                ShaderGraph::RegisterNodeType("Subtract",       []() -> SGNodePtr { return std::make_shared<SGSubtractNode>(); });
                ShaderGraph::RegisterNodeType("Multiply",       []() -> SGNodePtr { return std::make_shared<SGMultiplyNode>(); });
                ShaderGraph::RegisterNodeType("Divide",         []() -> SGNodePtr { return std::make_shared<SGDivideNode>(); });
                ShaderGraph::RegisterNodeType("Lerp",           []() -> SGNodePtr { return std::make_shared<SGLerpNode>(); });
                ShaderGraph::RegisterNodeType("Clamp",          []() -> SGNodePtr { return std::make_shared<SGClampNode>(); });
                ShaderGraph::RegisterNodeType("Saturate",       []() -> SGNodePtr { return std::make_shared<SGSaturateNode>(); });
                ShaderGraph::RegisterNodeType("Abs",            []() -> SGNodePtr { return std::make_shared<SGAbsNode>(); });
                ShaderGraph::RegisterNodeType("Power",          []() -> SGNodePtr { return std::make_shared<SGPowerNode>(); });
                ShaderGraph::RegisterNodeType("Dot",            []() -> SGNodePtr { return std::make_shared<SGDotNode>(); });
                ShaderGraph::RegisterNodeType("Cross",          []() -> SGNodePtr { return std::make_shared<SGCrossNode>(); });
                ShaderGraph::RegisterNodeType("Normalize",      []() -> SGNodePtr { return std::make_shared<SGNormalizeNode>(); });
                ShaderGraph::RegisterNodeType("ComponentMask",  []() -> SGNodePtr { return std::make_shared<SGComponentMaskNode>(); });
                ShaderGraph::RegisterNodeType("Append",         []() -> SGNodePtr { return std::make_shared<SGAppendNode>(); });

                // Input nodes
                ShaderGraph::RegisterNodeType("TexCoord",       []() -> SGNodePtr { return std::make_shared<SGTexCoordNode>(); });
                ShaderGraph::RegisterNodeType("VertexColor",    []() -> SGNodePtr { return std::make_shared<SGVertexColorNode>(); });
                ShaderGraph::RegisterNodeType("WorldPosition",  []() -> SGNodePtr { return std::make_shared<SGWorldPositionNode>(); });
                ShaderGraph::RegisterNodeType("WorldNormal",    []() -> SGNodePtr { return std::make_shared<SGWorldNormalNode>(); });
                ShaderGraph::RegisterNodeType("Time",           []() -> SGNodePtr { return std::make_shared<SGTimeNode>(); });
                ShaderGraph::RegisterNodeType("ConstantFloat",  []() -> SGNodePtr { return std::make_shared<SGConstantFloatNode>(); });
                ShaderGraph::RegisterNodeType("ConstantVec2",   []() -> SGNodePtr { return std::make_shared<SGConstantVec2Node>(); });
                ShaderGraph::RegisterNodeType("ConstantVec3",   []() -> SGNodePtr { return std::make_shared<SGConstantVec3Node>(); });
                ShaderGraph::RegisterNodeType("ConstantVec4",   []() -> SGNodePtr { return std::make_shared<SGConstantVec4Node>(); });
                ShaderGraph::RegisterNodeType("ScalarParam",    []() -> SGNodePtr { return std::make_shared<SGScalarParamNode>(); });
                ShaderGraph::RegisterNodeType("VectorParam",    []() -> SGNodePtr { return std::make_shared<SGVectorParamNode>(); });
                ShaderGraph::RegisterNodeType("TextureParam",   []() -> SGNodePtr { return std::make_shared<SGTextureParamNode>(); });
                ShaderGraph::RegisterNodeType("TextureSample",  []() -> SGNodePtr { return std::make_shared<SGTextureSampleNode>(); });

                // Output
                ShaderGraph::RegisterNodeType("MaterialOutput", []() -> SGNodePtr { return std::make_shared<SGMaterialOutputNode>(); });
            }
        };

        static SGNodeTypeRegistrar sRegistrar;
    }

} // namespace sky::sg

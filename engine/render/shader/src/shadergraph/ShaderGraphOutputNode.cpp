//
// Created by blues on 2026/3/10.
//

#include <shader/shadergraph/ShaderGraphOutputNode.h>
#include <framework/serialization/JsonArchive.h>

namespace sky::sg {

    SGMaterialOutputNode::SGMaterialOutputNode()
    {
        name = "MaterialOutput";
        // Input pins match PBR material slots (following MaterialSlot enum order)
        inputPins.push_back({"BaseColor",   SGDataType::FLOAT3, SGPinDirection::INPUT});
        inputPins.push_back({"Metallic",    SGDataType::FLOAT,  SGPinDirection::INPUT});
        inputPins.push_back({"Roughness",   SGDataType::FLOAT,  SGPinDirection::INPUT});
        inputPins.push_back({"Normal",      SGDataType::FLOAT3, SGPinDirection::INPUT});
        inputPins.push_back({"Emissive",    SGDataType::FLOAT3, SGPinDirection::INPUT});
        inputPins.push_back({"Opacity",     SGDataType::FLOAT,  SGPinDirection::INPUT});
        inputPins.push_back({"OpacityMask", SGDataType::FLOAT,  SGPinDirection::INPUT});
        // No output pins – this is the terminal node
    }

    void SGMaterialOutputNode::GenerateHLSL(const std::vector<std::string>& inputVars,
                                             std::vector<std::string>& /*outputVars*/,
                                             SGCodeGenContext& ctx) const
    {
        static const char* SLOTS[] = {
            "BaseColor", "Metallic", "Roughness", "Normal", "Emissive", "Opacity", "OpacityMask"
        };

        for (size_t i = 0; i < inputPins.size(); ++i) {
            if (i < inputVars.size() && !inputVars[i].empty()) {
                ctx.bodyCode += "    surface." + std::string(SLOTS[i]) + " = " + inputVars[i] + ";\n";
            }
        }
    }

} // namespace sky::sg

//
// Created by blues on 2026/3/10.
//

#include <shader/shadergraph/ShaderGraphInputNodes.h>
#include <framework/serialization/JsonArchive.h>
#include <sstream>

namespace sky::sg {

    // ---- SGTexCoordNode ----

    SGTexCoordNode::SGTexCoordNode(uint32_t idx) : uvIndex(idx)
    {
        name = "TexCoord";
        outputPins.push_back({"UV", SGDataType::FLOAT2, SGPinDirection::OUTPUT});
    }

    void SGTexCoordNode::SetUVIndex(uint32_t idx)
    {
        uvIndex = idx;
    }

    void SGTexCoordNode::LoadJson(JsonInputArchive& archive)
    {
        SGNode::LoadJson(archive);
        archive.LoadKeyValue("uvIndex", uvIndex);
    }

    void SGTexCoordNode::SaveJson(JsonOutputArchive& archive) const
    {
        SGNode::SaveJson(archive);
        archive.Key("uvIndex"); archive.SaveValue(uvIndex);
    }

    void SGTexCoordNode::GenerateHLSL(const std::vector<std::string>& /*inputVars*/,
                                       std::vector<std::string>& outputVars,
                                       SGCodeGenContext& ctx) const
    {
        std::string varName = ctx.NextVarName();
        ctx.bodyCode += "    float2 " + varName + " = input.UV" + (uvIndex > 0 ? std::to_string(uvIndex) : "") + ";\n";
        outputVars.push_back(varName);
    }

    // ---- SGVertexColorNode ----

    SGVertexColorNode::SGVertexColorNode()
    {
        name = "VertexColor";
        outputPins.push_back({"Color", SGDataType::FLOAT4, SGPinDirection::OUTPUT});
    }

    void SGVertexColorNode::GenerateHLSL(const std::vector<std::string>& /*inputVars*/,
                                          std::vector<std::string>& outputVars,
                                          SGCodeGenContext& ctx) const
    {
        std::string varName = ctx.NextVarName();
        ctx.bodyCode += "    float4 " + varName + " = input.Color;\n";
        outputVars.push_back(varName);
    }

    // ---- SGWorldPositionNode ----

    SGWorldPositionNode::SGWorldPositionNode()
    {
        name = "WorldPosition";
        outputPins.push_back({"Position", SGDataType::FLOAT3, SGPinDirection::OUTPUT});
    }

    void SGWorldPositionNode::GenerateHLSL(const std::vector<std::string>& /*inputVars*/,
                                            std::vector<std::string>& outputVars,
                                            SGCodeGenContext& ctx) const
    {
        std::string varName = ctx.NextVarName();
        ctx.bodyCode += "    float3 " + varName + " = input.WorldPos;\n";
        outputVars.push_back(varName);
    }

    // ---- SGWorldNormalNode ----

    SGWorldNormalNode::SGWorldNormalNode()
    {
        name = "WorldNormal";
        outputPins.push_back({"Normal", SGDataType::FLOAT3, SGPinDirection::OUTPUT});
    }

    void SGWorldNormalNode::GenerateHLSL(const std::vector<std::string>& /*inputVars*/,
                                          std::vector<std::string>& outputVars,
                                          SGCodeGenContext& ctx) const
    {
        std::string varName = ctx.NextVarName();
        ctx.bodyCode += "    float3 " + varName + " = input.WorldNormal;\n";
        outputVars.push_back(varName);
    }

    // ---- SGTimeNode ----

    SGTimeNode::SGTimeNode()
    {
        name = "Time";
        outputPins.push_back({"Time",   SGDataType::FLOAT, SGPinDirection::OUTPUT});
        outputPins.push_back({"SinTime", SGDataType::FLOAT, SGPinDirection::OUTPUT});
        outputPins.push_back({"CosTime", SGDataType::FLOAT, SGPinDirection::OUTPUT});
    }

    void SGTimeNode::GenerateHLSL(const std::vector<std::string>& /*inputVars*/,
                                   std::vector<std::string>& outputVars,
                                   SGCodeGenContext& ctx) const
    {
        std::string t   = ctx.NextVarName();
        std::string st  = ctx.NextVarName();
        std::string ct  = ctx.NextVarName();
        ctx.bodyCode += "    float " + t  + " = _Time.y;\n";
        ctx.bodyCode += "    float " + st + " = sin(_Time.y);\n";
        ctx.bodyCode += "    float " + ct + " = cos(_Time.y);\n";
        outputVars.push_back(t);
        outputVars.push_back(st);
        outputVars.push_back(ct);
    }

    // ---- SGConstantFloatNode ----

    SGConstantFloatNode::SGConstantFloatNode(float v) : value(v)
    {
        name = "ConstantFloat";
        outputPins.push_back({"Value", SGDataType::FLOAT, SGPinDirection::OUTPUT});
    }

    void SGConstantFloatNode::LoadJson(JsonInputArchive& archive)
    {
        SGNode::LoadJson(archive);
        double v = 0.0;
        archive.LoadKeyValue("value", v);
        value = static_cast<float>(v);
    }

    void SGConstantFloatNode::SaveJson(JsonOutputArchive& archive) const
    {
        SGNode::SaveJson(archive);
        archive.Key("value"); archive.SaveValue(static_cast<double>(value));
    }

    void SGConstantFloatNode::GenerateHLSL(const std::vector<std::string>& /*inputVars*/,
                                            std::vector<std::string>& outputVars,
                                            SGCodeGenContext& ctx) const
    {
        std::string varName = ctx.NextVarName();
        std::ostringstream ss;
        ss << "    float " << varName << " = " << value << ";\n";
        ctx.bodyCode += ss.str();
        outputVars.push_back(varName);
    }

    // ---- SGConstantVec2Node ----

    SGConstantVec2Node::SGConstantVec2Node(float x, float y) : value({x, y})
    {
        name = "ConstantVec2";
        outputPins.push_back({"Value", SGDataType::FLOAT2, SGPinDirection::OUTPUT});
    }

    void SGConstantVec2Node::LoadJson(JsonInputArchive& archive)
    {
        SGNode::LoadJson(archive);
        double x = 0.0, y = 0.0;
        archive.LoadKeyValue("x", x);
        archive.LoadKeyValue("y", y);
        value[0] = static_cast<float>(x);
        value[1] = static_cast<float>(y);
    }

    void SGConstantVec2Node::SaveJson(JsonOutputArchive& archive) const
    {
        SGNode::SaveJson(archive);
        archive.Key("x"); archive.SaveValue(static_cast<double>(value[0]));
        archive.Key("y"); archive.SaveValue(static_cast<double>(value[1]));
    }

    void SGConstantVec2Node::GenerateHLSL(const std::vector<std::string>& /*inputVars*/,
                                           std::vector<std::string>& outputVars,
                                           SGCodeGenContext& ctx) const
    {
        std::string varName = ctx.NextVarName();
        std::ostringstream ss;
        ss << "    float2 " << varName << " = float2(" << value[0] << ", " << value[1] << ");\n";
        ctx.bodyCode += ss.str();
        outputVars.push_back(varName);
    }

    // ---- SGConstantVec3Node ----

    SGConstantVec3Node::SGConstantVec3Node(float x, float y, float z) : value({x, y, z})
    {
        name = "ConstantVec3";
        outputPins.push_back({"Value", SGDataType::FLOAT3, SGPinDirection::OUTPUT});
    }

    void SGConstantVec3Node::LoadJson(JsonInputArchive& archive)
    {
        SGNode::LoadJson(archive);
        double x = 0.0, y = 0.0, z = 0.0;
        archive.LoadKeyValue("x", x);
        archive.LoadKeyValue("y", y);
        archive.LoadKeyValue("z", z);
        value[0] = static_cast<float>(x);
        value[1] = static_cast<float>(y);
        value[2] = static_cast<float>(z);
    }

    void SGConstantVec3Node::SaveJson(JsonOutputArchive& archive) const
    {
        SGNode::SaveJson(archive);
        archive.Key("x"); archive.SaveValue(static_cast<double>(value[0]));
        archive.Key("y"); archive.SaveValue(static_cast<double>(value[1]));
        archive.Key("z"); archive.SaveValue(static_cast<double>(value[2]));
    }

    void SGConstantVec3Node::GenerateHLSL(const std::vector<std::string>& /*inputVars*/,
                                           std::vector<std::string>& outputVars,
                                           SGCodeGenContext& ctx) const
    {
        std::string varName = ctx.NextVarName();
        std::ostringstream ss;
        ss << "    float3 " << varName << " = float3(" << value[0] << ", " << value[1] << ", " << value[2] << ");\n";
        ctx.bodyCode += ss.str();
        outputVars.push_back(varName);
    }

    // ---- SGConstantVec4Node ----

    SGConstantVec4Node::SGConstantVec4Node(float x, float y, float z, float w) : value({x, y, z, w})
    {
        name = "ConstantVec4";
        outputPins.push_back({"Value", SGDataType::FLOAT4, SGPinDirection::OUTPUT});
    }

    void SGConstantVec4Node::LoadJson(JsonInputArchive& archive)
    {
        SGNode::LoadJson(archive);
        double x = 0.0, y = 0.0, z = 0.0, w = 0.0;
        archive.LoadKeyValue("x", x);
        archive.LoadKeyValue("y", y);
        archive.LoadKeyValue("z", z);
        archive.LoadKeyValue("w", w);
        value[0] = static_cast<float>(x);
        value[1] = static_cast<float>(y);
        value[2] = static_cast<float>(z);
        value[3] = static_cast<float>(w);
    }

    void SGConstantVec4Node::SaveJson(JsonOutputArchive& archive) const
    {
        SGNode::SaveJson(archive);
        archive.Key("x"); archive.SaveValue(static_cast<double>(value[0]));
        archive.Key("y"); archive.SaveValue(static_cast<double>(value[1]));
        archive.Key("z"); archive.SaveValue(static_cast<double>(value[2]));
        archive.Key("w"); archive.SaveValue(static_cast<double>(value[3]));
    }

    void SGConstantVec4Node::GenerateHLSL(const std::vector<std::string>& /*inputVars*/,
                                           std::vector<std::string>& outputVars,
                                           SGCodeGenContext& ctx) const
    {
        std::string varName = ctx.NextVarName();
        std::ostringstream ss;
        ss << "    float4 " << varName << " = float4("
           << value[0] << ", " << value[1] << ", " << value[2] << ", " << value[3] << ");\n";
        ctx.bodyCode += ss.str();
        outputVars.push_back(varName);
    }

    // ---- SGScalarParamNode ----

    SGScalarParamNode::SGScalarParamNode(const std::string& pName, float defVal)
        : paramName(pName), defaultValue(defVal)
    {
        name = "ScalarParam";
        outputPins.push_back({"Value", SGDataType::FLOAT, SGPinDirection::OUTPUT});
    }

    void SGScalarParamNode::SetParamName(const std::string& n)
    {
        paramName = n;
    }

    void SGScalarParamNode::LoadJson(JsonInputArchive& archive)
    {
        SGNode::LoadJson(archive);
        archive.LoadKeyValue("paramName", paramName);
        double dv = 0.0;
        archive.LoadKeyValue("defaultValue", dv);
        defaultValue = static_cast<float>(dv);
    }

    void SGScalarParamNode::SaveJson(JsonOutputArchive& archive) const
    {
        SGNode::SaveJson(archive);
        archive.SaveValueObject("paramName", paramName);
        archive.Key("defaultValue"); archive.SaveValue(static_cast<double>(defaultValue));
    }

    void SGScalarParamNode::GenerateHLSL(const std::vector<std::string>& /*inputVars*/,
                                          std::vector<std::string>& outputVars,
                                          SGCodeGenContext& ctx) const
    {
        // Emit a global cbuffer member declaration (once per unique param)
        std::string decl = "float " + paramName + ";\n";
        if (ctx.declarations.find(decl) == std::string::npos) {
            ctx.declarations += decl;
        }
        outputVars.push_back(paramName);
    }

    // ---- SGVectorParamNode ----

    SGVectorParamNode::SGVectorParamNode(const std::string& pName) : paramName(pName)
    {
        name = "VectorParam";
        outputPins.push_back({"Value",  SGDataType::FLOAT4, SGPinDirection::OUTPUT});
        outputPins.push_back({"RGB",    SGDataType::FLOAT3, SGPinDirection::OUTPUT});
        outputPins.push_back({"R",      SGDataType::FLOAT,  SGPinDirection::OUTPUT});
        outputPins.push_back({"G",      SGDataType::FLOAT,  SGPinDirection::OUTPUT});
        outputPins.push_back({"B",      SGDataType::FLOAT,  SGPinDirection::OUTPUT});
        outputPins.push_back({"A",      SGDataType::FLOAT,  SGPinDirection::OUTPUT});
    }

    void SGVectorParamNode::SetParamName(const std::string& n)
    {
        paramName = n;
    }

    void SGVectorParamNode::LoadJson(JsonInputArchive& archive)
    {
        SGNode::LoadJson(archive);
        archive.LoadKeyValue("paramName", paramName);
    }

    void SGVectorParamNode::SaveJson(JsonOutputArchive& archive) const
    {
        SGNode::SaveJson(archive);
        archive.SaveValueObject("paramName", paramName);
    }

    void SGVectorParamNode::GenerateHLSL(const std::vector<std::string>& /*inputVars*/,
                                          std::vector<std::string>& outputVars,
                                          SGCodeGenContext& ctx) const
    {
        std::string decl = "float4 " + paramName + ";\n";
        if (ctx.declarations.find(decl) == std::string::npos) {
            ctx.declarations += decl;
        }
        outputVars.push_back(paramName);
        outputVars.push_back(paramName + ".rgb");
        outputVars.push_back(paramName + ".r");
        outputVars.push_back(paramName + ".g");
        outputVars.push_back(paramName + ".b");
        outputVars.push_back(paramName + ".a");
    }

    // ---- SGTextureParamNode ----

    SGTextureParamNode::SGTextureParamNode(const std::string& pName) : paramName(pName)
    {
        name = "TextureParam";
        outputPins.push_back({"Texture", SGDataType::TEXTURE2D,     SGPinDirection::OUTPUT});
        outputPins.push_back({"Sampler", SGDataType::SAMPLER_STATE, SGPinDirection::OUTPUT});
    }

    void SGTextureParamNode::SetParamName(const std::string& n)
    {
        paramName = n;
    }

    void SGTextureParamNode::LoadJson(JsonInputArchive& archive)
    {
        SGNode::LoadJson(archive);
        archive.LoadKeyValue("paramName", paramName);
    }

    void SGTextureParamNode::SaveJson(JsonOutputArchive& archive) const
    {
        SGNode::SaveJson(archive);
        archive.SaveValueObject("paramName", paramName);
    }

    void SGTextureParamNode::GenerateHLSL(const std::vector<std::string>& /*inputVars*/,
                                           std::vector<std::string>& outputVars,
                                           SGCodeGenContext& ctx) const
    {
        std::string texDecl     = "Texture2D " + paramName + ";\n";
        std::string samplerDecl = "SamplerState " + paramName + "Sampler;\n";
        if (ctx.declarations.find(texDecl) == std::string::npos) {
            ctx.declarations += texDecl;
        }
        if (ctx.declarations.find(samplerDecl) == std::string::npos) {
            ctx.declarations += samplerDecl;
        }
        outputVars.push_back(paramName);
        outputVars.push_back(paramName + "Sampler");
    }

    // ---- SGTextureSampleNode ----

    SGTextureSampleNode::SGTextureSampleNode()
    {
        name = "TextureSample";
        inputPins.push_back({"Texture", SGDataType::TEXTURE2D,     SGPinDirection::INPUT});
        inputPins.push_back({"Sampler", SGDataType::SAMPLER_STATE, SGPinDirection::INPUT});
        inputPins.push_back({"UV",      SGDataType::FLOAT2,        SGPinDirection::INPUT});
        outputPins.push_back({"RGBA", SGDataType::FLOAT4, SGPinDirection::OUTPUT});
        outputPins.push_back({"RGB",  SGDataType::FLOAT3, SGPinDirection::OUTPUT});
        outputPins.push_back({"R",    SGDataType::FLOAT,  SGPinDirection::OUTPUT});
        outputPins.push_back({"G",    SGDataType::FLOAT,  SGPinDirection::OUTPUT});
        outputPins.push_back({"B",    SGDataType::FLOAT,  SGPinDirection::OUTPUT});
        outputPins.push_back({"A",    SGDataType::FLOAT,  SGPinDirection::OUTPUT});
    }

    void SGTextureSampleNode::GenerateHLSL(const std::vector<std::string>& inputVars,
                                            std::vector<std::string>& outputVars,
                                            SGCodeGenContext& ctx) const
    {
        std::string varName = ctx.NextVarName();
        std::string tex     = (inputVars.size() > 0 && !inputVars[0].empty()) ? inputVars[0] : "_DefaultTex";
        std::string sampler = (inputVars.size() > 1 && !inputVars[1].empty()) ? inputVars[1] : "_DefaultSampler";
        std::string uv      = (inputVars.size() > 2 && !inputVars[2].empty()) ? inputVars[2] : "float2(0,0)";

        ctx.bodyCode += "    float4 " + varName + " = " + tex + ".Sample(" + sampler + ", " + uv + ");\n";
        outputVars.push_back(varName);
        outputVars.push_back(varName + ".rgb");
        outputVars.push_back(varName + ".r");
        outputVars.push_back(varName + ".g");
        outputVars.push_back(varName + ".b");
        outputVars.push_back(varName + ".a");
    }

} // namespace sky::sg

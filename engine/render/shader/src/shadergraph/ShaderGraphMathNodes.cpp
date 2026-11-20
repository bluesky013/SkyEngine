//
// Created by blues on 2026/3/10.
//

#include <shader/shadergraph/ShaderGraphMathNodes.h>
#include <framework/serialization/JsonArchive.h>
#include <sstream>

namespace sky::sg {

    // ---- SGBinaryMathNode ----

    SGBinaryMathNode::SGBinaryMathNode(SGDataType type) : dataType(type)
    {
        SetDataType(type);
    }

    void SGBinaryMathNode::SetDataType(SGDataType type)
    {
        dataType = type;
        inputPins.clear();
        outputPins.clear();
        inputPins.push_back({"A", type, SGPinDirection::INPUT});
        inputPins.push_back({"B", type, SGPinDirection::INPUT});
        outputPins.push_back({"Output", type, SGPinDirection::OUTPUT});
    }

    void SGBinaryMathNode::LoadJson(JsonInputArchive& archive)
    {
        SGNode::LoadJson(archive);
        uint8_t dt = 0;
        archive.LoadKeyValue("dataType", dt);
        SetDataType(static_cast<SGDataType>(dt));
    }

    void SGBinaryMathNode::SaveJson(JsonOutputArchive& archive) const
    {
        SGNode::SaveJson(archive);
        archive.Key("dataType"); archive.SaveValue(static_cast<uint32_t>(dataType));
    }

    void SGBinaryMathNode::GenerateBinary(const std::vector<std::string>& inputVars,
                                          std::vector<std::string>& outputVars,
                                          SGCodeGenContext& ctx,
                                          const char* op) const
    {
        std::string varName = ctx.NextVarName();
        std::string hlslType = SGDataTypeToHLSL(dataType);

        std::string a = (inputVars.size() > 0 && !inputVars[0].empty()) ? inputVars[0] : "0.0";
        std::string b = (inputVars.size() > 1 && !inputVars[1].empty()) ? inputVars[1] : "0.0";

        ctx.bodyCode += "    " + hlslType + " " + varName + " = " + a + " " + op + " " + b + ";\n";
        outputVars.push_back(varName);
    }

    // ---- SGAddNode ----

    SGAddNode::SGAddNode(SGDataType type) : SGBinaryMathNode(type) { name = "Add"; }

    void SGAddNode::GenerateHLSL(const std::vector<std::string>& inputVars,
                                  std::vector<std::string>& outputVars,
                                  SGCodeGenContext& ctx) const
    {
        GenerateBinary(inputVars, outputVars, ctx, "+");
    }

    // ---- SGSubtractNode ----

    SGSubtractNode::SGSubtractNode(SGDataType type) : SGBinaryMathNode(type) { name = "Subtract"; }

    void SGSubtractNode::GenerateHLSL(const std::vector<std::string>& inputVars,
                                       std::vector<std::string>& outputVars,
                                       SGCodeGenContext& ctx) const
    {
        GenerateBinary(inputVars, outputVars, ctx, "-");
    }

    // ---- SGMultiplyNode ----

    SGMultiplyNode::SGMultiplyNode(SGDataType type) : SGBinaryMathNode(type) { name = "Multiply"; }

    void SGMultiplyNode::GenerateHLSL(const std::vector<std::string>& inputVars,
                                       std::vector<std::string>& outputVars,
                                       SGCodeGenContext& ctx) const
    {
        GenerateBinary(inputVars, outputVars, ctx, "*");
    }

    // ---- SGDivideNode ----

    SGDivideNode::SGDivideNode(SGDataType type) : SGBinaryMathNode(type) { name = "Divide"; }

    void SGDivideNode::GenerateHLSL(const std::vector<std::string>& inputVars,
                                     std::vector<std::string>& outputVars,
                                     SGCodeGenContext& ctx) const
    {
        GenerateBinary(inputVars, outputVars, ctx, "/");
    }

    // ---- SGLerpNode ----

    SGLerpNode::SGLerpNode(SGDataType type) : dataType(type)
    {
        name = "Lerp";
        SetDataType(type);
    }

    void SGLerpNode::SetDataType(SGDataType type)
    {
        dataType = type;
        inputPins.clear();
        outputPins.clear();
        inputPins.push_back({"A",     type,              SGPinDirection::INPUT});
        inputPins.push_back({"B",     type,              SGPinDirection::INPUT});
        inputPins.push_back({"Alpha", SGDataType::FLOAT, SGPinDirection::INPUT});
        outputPins.push_back({"Output", type, SGPinDirection::OUTPUT});
    }

    void SGLerpNode::LoadJson(JsonInputArchive& archive)
    {
        SGNode::LoadJson(archive);
        uint8_t dt = 0;
        archive.LoadKeyValue("dataType", dt);
        SetDataType(static_cast<SGDataType>(dt));
    }

    void SGLerpNode::SaveJson(JsonOutputArchive& archive) const
    {
        SGNode::SaveJson(archive);
        archive.Key("dataType"); archive.SaveValue(static_cast<uint32_t>(dataType));
    }

    void SGLerpNode::GenerateHLSL(const std::vector<std::string>& inputVars,
                                   std::vector<std::string>& outputVars,
                                   SGCodeGenContext& ctx) const
    {
        std::string varName  = ctx.NextVarName();
        std::string hlslType = SGDataTypeToHLSL(dataType);
        std::string a     = (inputVars.size() > 0 && !inputVars[0].empty()) ? inputVars[0] : "0.0";
        std::string b     = (inputVars.size() > 1 && !inputVars[1].empty()) ? inputVars[1] : "0.0";
        std::string alpha = (inputVars.size() > 2 && !inputVars[2].empty()) ? inputVars[2] : "0.5";

        ctx.bodyCode += "    " + hlslType + " " + varName + " = lerp(" + a + ", " + b + ", " + alpha + ");\n";
        outputVars.push_back(varName);
    }

    // ---- SGClampNode ----

    SGClampNode::SGClampNode(SGDataType type) : dataType(type)
    {
        name = "Clamp";
        SetDataType(type);
    }

    void SGClampNode::SetDataType(SGDataType type)
    {
        dataType = type;
        inputPins.clear();
        outputPins.clear();
        inputPins.push_back({"Value", type, SGPinDirection::INPUT});
        inputPins.push_back({"Min",   type, SGPinDirection::INPUT});
        inputPins.push_back({"Max",   type, SGPinDirection::INPUT});
        outputPins.push_back({"Output", type, SGPinDirection::OUTPUT});
    }

    void SGClampNode::LoadJson(JsonInputArchive& archive)
    {
        SGNode::LoadJson(archive);
        uint8_t dt = 0;
        archive.LoadKeyValue("dataType", dt);
        SetDataType(static_cast<SGDataType>(dt));
    }

    void SGClampNode::SaveJson(JsonOutputArchive& archive) const
    {
        SGNode::SaveJson(archive);
        archive.Key("dataType"); archive.SaveValue(static_cast<uint32_t>(dataType));
    }

    void SGClampNode::GenerateHLSL(const std::vector<std::string>& inputVars,
                                    std::vector<std::string>& outputVars,
                                    SGCodeGenContext& ctx) const
    {
        std::string varName  = ctx.NextVarName();
        std::string hlslType = SGDataTypeToHLSL(dataType);
        std::string val = (inputVars.size() > 0 && !inputVars[0].empty()) ? inputVars[0] : "0.0";
        std::string mn  = (inputVars.size() > 1 && !inputVars[1].empty()) ? inputVars[1] : "0.0";
        std::string mx  = (inputVars.size() > 2 && !inputVars[2].empty()) ? inputVars[2] : "1.0";

        ctx.bodyCode += "    " + hlslType + " " + varName + " = clamp(" + val + ", " + mn + ", " + mx + ");\n";
        outputVars.push_back(varName);
    }

    // ---- SGSaturateNode ----

    SGSaturateNode::SGSaturateNode(SGDataType type) : dataType(type)
    {
        name = "Saturate";
        SetDataType(type);
    }

    void SGSaturateNode::SetDataType(SGDataType type)
    {
        dataType = type;
        inputPins.clear();
        outputPins.clear();
        inputPins.push_back({"Value", type, SGPinDirection::INPUT});
        outputPins.push_back({"Output", type, SGPinDirection::OUTPUT});
    }

    void SGSaturateNode::LoadJson(JsonInputArchive& archive)
    {
        SGNode::LoadJson(archive);
        uint8_t dt = 0;
        archive.LoadKeyValue("dataType", dt);
        SetDataType(static_cast<SGDataType>(dt));
    }

    void SGSaturateNode::SaveJson(JsonOutputArchive& archive) const
    {
        SGNode::SaveJson(archive);
        archive.Key("dataType"); archive.SaveValue(static_cast<uint32_t>(dataType));
    }

    void SGSaturateNode::GenerateHLSL(const std::vector<std::string>& inputVars,
                                       std::vector<std::string>& outputVars,
                                       SGCodeGenContext& ctx) const
    {
        std::string varName  = ctx.NextVarName();
        std::string hlslType = SGDataTypeToHLSL(dataType);
        std::string val = (inputVars.size() > 0 && !inputVars[0].empty()) ? inputVars[0] : "0.0";

        ctx.bodyCode += "    " + hlslType + " " + varName + " = saturate(" + val + ");\n";
        outputVars.push_back(varName);
    }

    // ---- SGAbsNode ----

    SGAbsNode::SGAbsNode(SGDataType type) : dataType(type)
    {
        name = "Abs";
        SetDataType(type);
    }

    void SGAbsNode::SetDataType(SGDataType type)
    {
        dataType = type;
        inputPins.clear();
        outputPins.clear();
        inputPins.push_back({"Value", type, SGPinDirection::INPUT});
        outputPins.push_back({"Output", type, SGPinDirection::OUTPUT});
    }

    void SGAbsNode::LoadJson(JsonInputArchive& archive)
    {
        SGNode::LoadJson(archive);
        uint8_t dt = 0;
        archive.LoadKeyValue("dataType", dt);
        SetDataType(static_cast<SGDataType>(dt));
    }

    void SGAbsNode::SaveJson(JsonOutputArchive& archive) const
    {
        SGNode::SaveJson(archive);
        archive.Key("dataType"); archive.SaveValue(static_cast<uint32_t>(dataType));
    }

    void SGAbsNode::GenerateHLSL(const std::vector<std::string>& inputVars,
                                   std::vector<std::string>& outputVars,
                                   SGCodeGenContext& ctx) const
    {
        std::string varName  = ctx.NextVarName();
        std::string hlslType = SGDataTypeToHLSL(dataType);
        std::string val = (inputVars.size() > 0 && !inputVars[0].empty()) ? inputVars[0] : "0.0";

        ctx.bodyCode += "    " + hlslType + " " + varName + " = abs(" + val + ");\n";
        outputVars.push_back(varName);
    }

    // ---- SGPowerNode ----

    SGPowerNode::SGPowerNode()
    {
        name = "Power";
        inputPins.push_back({"Base", SGDataType::FLOAT, SGPinDirection::INPUT});
        inputPins.push_back({"Exp",  SGDataType::FLOAT, SGPinDirection::INPUT});
        outputPins.push_back({"Output", SGDataType::FLOAT, SGPinDirection::OUTPUT});
    }

    void SGPowerNode::GenerateHLSL(const std::vector<std::string>& inputVars,
                                    std::vector<std::string>& outputVars,
                                    SGCodeGenContext& ctx) const
    {
        std::string varName = ctx.NextVarName();
        std::string base = (inputVars.size() > 0 && !inputVars[0].empty()) ? inputVars[0] : "0.0";
        std::string exp  = (inputVars.size() > 1 && !inputVars[1].empty()) ? inputVars[1] : "1.0";

        ctx.bodyCode += "    float " + varName + " = pow(" + base + ", " + exp + ");\n";
        outputVars.push_back(varName);
    }

    // ---- SGDotNode ----

    SGDotNode::SGDotNode(SGDataType type) : dataType(type)
    {
        name = "Dot";
        SetDataType(type);
    }

    void SGDotNode::SetDataType(SGDataType type)
    {
        dataType = type;
        inputPins.clear();
        outputPins.clear();
        inputPins.push_back({"A", type, SGPinDirection::INPUT});
        inputPins.push_back({"B", type, SGPinDirection::INPUT});
        outputPins.push_back({"Output", SGDataType::FLOAT, SGPinDirection::OUTPUT});
    }

    void SGDotNode::LoadJson(JsonInputArchive& archive)
    {
        SGNode::LoadJson(archive);
        uint8_t dt = 0;
        archive.LoadKeyValue("dataType", dt);
        SetDataType(static_cast<SGDataType>(dt));
    }

    void SGDotNode::SaveJson(JsonOutputArchive& archive) const
    {
        SGNode::SaveJson(archive);
        archive.Key("dataType"); archive.SaveValue(static_cast<uint32_t>(dataType));
    }

    void SGDotNode::GenerateHLSL(const std::vector<std::string>& inputVars,
                                   std::vector<std::string>& outputVars,
                                   SGCodeGenContext& ctx) const
    {
        std::string varName = ctx.NextVarName();
        std::string a = (inputVars.size() > 0 && !inputVars[0].empty()) ? inputVars[0] : "0.0";
        std::string b = (inputVars.size() > 1 && !inputVars[1].empty()) ? inputVars[1] : "0.0";

        ctx.bodyCode += "    float " + varName + " = dot(" + a + ", " + b + ");\n";
        outputVars.push_back(varName);
    }

    // ---- SGCrossNode ----

    SGCrossNode::SGCrossNode()
    {
        name = "Cross";
        inputPins.push_back({"A", SGDataType::FLOAT3, SGPinDirection::INPUT});
        inputPins.push_back({"B", SGDataType::FLOAT3, SGPinDirection::INPUT});
        outputPins.push_back({"Output", SGDataType::FLOAT3, SGPinDirection::OUTPUT});
    }

    void SGCrossNode::GenerateHLSL(const std::vector<std::string>& inputVars,
                                    std::vector<std::string>& outputVars,
                                    SGCodeGenContext& ctx) const
    {
        std::string varName = ctx.NextVarName();
        std::string a = (inputVars.size() > 0 && !inputVars[0].empty()) ? inputVars[0] : "float3(0,0,0)";
        std::string b = (inputVars.size() > 1 && !inputVars[1].empty()) ? inputVars[1] : "float3(0,0,0)";

        ctx.bodyCode += "    float3 " + varName + " = cross(" + a + ", " + b + ");\n";
        outputVars.push_back(varName);
    }

    // ---- SGNormalizeNode ----

    SGNormalizeNode::SGNormalizeNode()
    {
        name = "Normalize";
        inputPins.push_back({"V", SGDataType::FLOAT3, SGPinDirection::INPUT});
        outputPins.push_back({"Output", SGDataType::FLOAT3, SGPinDirection::OUTPUT});
    }

    void SGNormalizeNode::GenerateHLSL(const std::vector<std::string>& inputVars,
                                        std::vector<std::string>& outputVars,
                                        SGCodeGenContext& ctx) const
    {
        std::string varName = ctx.NextVarName();
        std::string v = (inputVars.size() > 0 && !inputVars[0].empty()) ? inputVars[0] : "float3(0,0,1)";

        ctx.bodyCode += "    float3 " + varName + " = normalize(" + v + ");\n";
        outputVars.push_back(varName);
    }

    // ---- SGComponentMaskNode ----

    SGComponentMaskNode::SGComponentMaskNode(SGDataType inType) : inputType(inType)
    {
        name = "ComponentMask";
        inputPins.push_back({"In", inType, SGPinDirection::INPUT});
        UpdateOutputPin();
    }

    void SGComponentMaskNode::SetChannels(bool r, bool g, bool b, bool a)
    {
        maskR = r; maskG = g; maskB = b; maskA = a;
        UpdateOutputPin();
    }

    void SGComponentMaskNode::UpdateOutputPin()
    {
        uint8_t count = (maskR ? 1 : 0) + (maskG ? 1 : 0) + (maskB ? 1 : 0) + (maskA ? 1 : 0);
        outputPins.clear();
        SGDataType outType = SGDataType::FLOAT;
        switch (count) {
            case 2: outType = SGDataType::FLOAT2; break;
            case 3: outType = SGDataType::FLOAT3; break;
            case 4: outType = SGDataType::FLOAT4; break;
            default: outType = SGDataType::FLOAT;  break;
        }
        outputPins.push_back({"Output", outType, SGPinDirection::OUTPUT});
    }

    void SGComponentMaskNode::LoadJson(JsonInputArchive& archive)
    {
        SGNode::LoadJson(archive);
        bool r = true, g = true, b = true, a = false;
        archive.LoadKeyValue("maskR", r);
        archive.LoadKeyValue("maskG", g);
        archive.LoadKeyValue("maskB", b);
        archive.LoadKeyValue("maskA", a);
        uint8_t dt = 0;
        archive.LoadKeyValue("inputType", dt);
        inputType = static_cast<SGDataType>(dt);
        inputPins[0].type = inputType;
        SetChannels(r, g, b, a);
    }

    void SGComponentMaskNode::SaveJson(JsonOutputArchive& archive) const
    {
        SGNode::SaveJson(archive);
        archive.Key("maskR"); archive.SaveValue(maskR);
        archive.Key("maskG"); archive.SaveValue(maskG);
        archive.Key("maskB"); archive.SaveValue(maskB);
        archive.Key("maskA"); archive.SaveValue(maskA);
        archive.Key("inputType"); archive.SaveValue(static_cast<uint32_t>(inputType));
    }

    void SGComponentMaskNode::GenerateHLSL(const std::vector<std::string>& inputVars,
                                             std::vector<std::string>& outputVars,
                                             SGCodeGenContext& ctx) const
    {
        std::string varName = ctx.NextVarName();
        std::string inVal = (inputVars.size() > 0 && !inputVars[0].empty()) ? inputVars[0] : "0.0";

        std::string swizzle;
        if (maskR) swizzle += 'r';
        if (maskG) swizzle += 'g';
        if (maskB) swizzle += 'b';
        if (maskA) swizzle += 'a';

        if (swizzle.empty()) swizzle = "r";

        uint8_t count = static_cast<uint8_t>(swizzle.size());
        SGDataType outType = SGDataType::FLOAT;
        switch (count) {
            case 2: outType = SGDataType::FLOAT2; break;
            case 3: outType = SGDataType::FLOAT3; break;
            case 4: outType = SGDataType::FLOAT4; break;
            default: outType = SGDataType::FLOAT;  break;
        }

        ctx.bodyCode += "    " + SGDataTypeToHLSL(outType) + " " + varName + " = " + inVal + "." + swizzle + ";\n";
        outputVars.push_back(varName);
    }

    // ---- SGAppendNode ----

    SGAppendNode::SGAppendNode(SGDataType aT, SGDataType bT) : aType(aT), bType(bT)
    {
        name = "Append";
        inputPins.push_back({"A", aT, SGPinDirection::INPUT});
        inputPins.push_back({"B", bT, SGPinDirection::INPUT});

        uint8_t totalComps = SGDataTypeComponents(aT) + SGDataTypeComponents(bT);
        SGDataType outType = SGDataType::FLOAT;
        switch (totalComps) {
            case 2: outType = SGDataType::FLOAT2; break;
            case 3: outType = SGDataType::FLOAT3; break;
            case 4: outType = SGDataType::FLOAT4; break;
            default: outType = SGDataType::FLOAT4; break;
        }
        outputPins.push_back({"Output", outType, SGPinDirection::OUTPUT});
    }

    void SGAppendNode::LoadJson(JsonInputArchive& archive)
    {
        SGNode::LoadJson(archive);
        uint8_t at = 0, bt = 0;
        archive.LoadKeyValue("aType", at);
        archive.LoadKeyValue("bType", bt);
        aType = static_cast<SGDataType>(at);
        bType = static_cast<SGDataType>(bt);
        inputPins[0].type = aType;
        inputPins[1].type = bType;
    }

    void SGAppendNode::SaveJson(JsonOutputArchive& archive) const
    {
        SGNode::SaveJson(archive);
        archive.Key("aType"); archive.SaveValue(static_cast<uint32_t>(aType));
        archive.Key("bType"); archive.SaveValue(static_cast<uint32_t>(bType));
    }

    void SGAppendNode::GenerateHLSL(const std::vector<std::string>& inputVars,
                                     std::vector<std::string>& outputVars,
                                     SGCodeGenContext& ctx) const
    {
        std::string varName = ctx.NextVarName();
        std::string a = (inputVars.size() > 0 && !inputVars[0].empty()) ? inputVars[0] : "0.0";
        std::string b = (inputVars.size() > 1 && !inputVars[1].empty()) ? inputVars[1] : "0.0";

        uint8_t totalComps = SGDataTypeComponents(aType) + SGDataTypeComponents(bType);
        SGDataType outType = SGDataType::FLOAT;
        switch (totalComps) {
            case 2: outType = SGDataType::FLOAT2; break;
            case 3: outType = SGDataType::FLOAT3; break;
            case 4: outType = SGDataType::FLOAT4; break;
            default: outType = SGDataType::FLOAT4; break;
        }

        ctx.bodyCode += "    " + SGDataTypeToHLSL(outType) + " " + varName +
                        " = " + SGDataTypeToHLSL(outType) + "(" + a + ", " + b + ");\n";
        outputVars.push_back(varName);
    }

} // namespace sky::sg

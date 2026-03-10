//
// Created by blues on 2026/3/10.
//

#pragma once

#include <shader/shadergraph/ShaderGraphNode.h>
#include <array>
#include <string>

namespace sky::sg {

    // TexCoord – outputs UV coordinates from vertex input
    class SGTexCoordNode : public SGNode {
    public:
        explicit SGTexCoordNode(uint32_t uvIndex = 0);
        ~SGTexCoordNode() override = default;

        std::string GetTypeName() const override    { return "TexCoord"; }
        std::string GetDisplayName() const override { return "Tex Coord"; }

        void SetUVIndex(uint32_t idx);
        void LoadJson(JsonInputArchive& archive) override;
        void SaveJson(JsonOutputArchive& archive) const override;
        void GenerateHLSL(const std::vector<std::string>& inputVars,
                          std::vector<std::string>& outputVars,
                          SGCodeGenContext& ctx) const override;
    private:
        uint32_t uvIndex = 0;
    };

    // VertexColor – outputs the per-vertex color (float4)
    class SGVertexColorNode : public SGNode {
    public:
        SGVertexColorNode();
        ~SGVertexColorNode() override = default;

        std::string GetTypeName() const override    { return "VertexColor"; }
        std::string GetDisplayName() const override { return "Vertex Color"; }
        void GenerateHLSL(const std::vector<std::string>& inputVars,
                          std::vector<std::string>& outputVars,
                          SGCodeGenContext& ctx) const override;
    };

    // WorldPosition – world-space position of the current pixel
    class SGWorldPositionNode : public SGNode {
    public:
        SGWorldPositionNode();
        ~SGWorldPositionNode() override = default;

        std::string GetTypeName() const override    { return "WorldPosition"; }
        std::string GetDisplayName() const override { return "World Position"; }
        void GenerateHLSL(const std::vector<std::string>& inputVars,
                          std::vector<std::string>& outputVars,
                          SGCodeGenContext& ctx) const override;
    };

    // WorldNormal – world-space normal vector
    class SGWorldNormalNode : public SGNode {
    public:
        SGWorldNormalNode();
        ~SGWorldNormalNode() override = default;

        std::string GetTypeName() const override    { return "WorldNormal"; }
        std::string GetDisplayName() const override { return "World Normal"; }
        void GenerateHLSL(const std::vector<std::string>& inputVars,
                          std::vector<std::string>& outputVars,
                          SGCodeGenContext& ctx) const override;
    };

    // Time – provides shader time value
    class SGTimeNode : public SGNode {
    public:
        SGTimeNode();
        ~SGTimeNode() override = default;

        std::string GetTypeName() const override    { return "Time"; }
        std::string GetDisplayName() const override { return "Time"; }
        void GenerateHLSL(const std::vector<std::string>& inputVars,
                          std::vector<std::string>& outputVars,
                          SGCodeGenContext& ctx) const override;
    };

    // ConstantFloat – a literal float value
    class SGConstantFloatNode : public SGNode {
    public:
        explicit SGConstantFloatNode(float value = 0.0f);
        ~SGConstantFloatNode() override = default;

        std::string GetTypeName() const override    { return "ConstantFloat"; }
        std::string GetDisplayName() const override { return "Constant (float)"; }

        void SetValue(float v) { value = v; }
        float GetValue() const { return value; }

        void LoadJson(JsonInputArchive& archive) override;
        void SaveJson(JsonOutputArchive& archive) const override;
        void GenerateHLSL(const std::vector<std::string>& inputVars,
                          std::vector<std::string>& outputVars,
                          SGCodeGenContext& ctx) const override;
    private:
        float value = 0.0f;
    };

    // ConstantVec2 – a literal float2 value
    class SGConstantVec2Node : public SGNode {
    public:
        explicit SGConstantVec2Node(float x = 0.0f, float y = 0.0f);
        ~SGConstantVec2Node() override = default;

        std::string GetTypeName() const override    { return "ConstantVec2"; }
        std::string GetDisplayName() const override { return "Constant (float2)"; }

        void SetValue(float x, float y) { value[0] = x; value[1] = y; }

        void LoadJson(JsonInputArchive& archive) override;
        void SaveJson(JsonOutputArchive& archive) const override;
        void GenerateHLSL(const std::vector<std::string>& inputVars,
                          std::vector<std::string>& outputVars,
                          SGCodeGenContext& ctx) const override;
    private:
        std::array<float, 2> value = {0.f, 0.f};
    };

    // ConstantVec3 – a literal float3 value
    class SGConstantVec3Node : public SGNode {
    public:
        explicit SGConstantVec3Node(float x = 0.0f, float y = 0.0f, float z = 0.0f);
        ~SGConstantVec3Node() override = default;

        std::string GetTypeName() const override    { return "ConstantVec3"; }
        std::string GetDisplayName() const override { return "Constant (float3)"; }

        void SetValue(float x, float y, float z) { value[0] = x; value[1] = y; value[2] = z; }

        void LoadJson(JsonInputArchive& archive) override;
        void SaveJson(JsonOutputArchive& archive) const override;
        void GenerateHLSL(const std::vector<std::string>& inputVars,
                          std::vector<std::string>& outputVars,
                          SGCodeGenContext& ctx) const override;
    private:
        std::array<float, 3> value = {0.f, 0.f, 0.f};
    };

    // ConstantVec4 – a literal float4 value
    class SGConstantVec4Node : public SGNode {
    public:
        explicit SGConstantVec4Node(float x = 0.0f, float y = 0.0f, float z = 0.0f, float w = 0.0f);
        ~SGConstantVec4Node() override = default;

        std::string GetTypeName() const override    { return "ConstantVec4"; }
        std::string GetDisplayName() const override { return "Constant (float4)"; }

        void SetValue(float x, float y, float z, float w) { value[0] = x; value[1] = y; value[2] = z; value[3] = w; }

        void LoadJson(JsonInputArchive& archive) override;
        void SaveJson(JsonOutputArchive& archive) const override;
        void GenerateHLSL(const std::vector<std::string>& inputVars,
                          std::vector<std::string>& outputVars,
                          SGCodeGenContext& ctx) const override;
    private:
        std::array<float, 4> value = {0.f, 0.f, 0.f, 0.f};
    };

    // ScalarParam – an exposed scalar (float) material parameter
    class SGScalarParamNode : public SGNode {
    public:
        explicit SGScalarParamNode(const std::string& paramName = "Param", float defaultVal = 0.0f);
        ~SGScalarParamNode() override = default;

        std::string GetTypeName() const override    { return "ScalarParam"; }
        std::string GetDisplayName() const override { return "Scalar Parameter"; }

        void SetParamName(const std::string& n);
        void SetDefaultValue(float v) { defaultValue = v; }
        const std::string& GetParamName() const { return paramName; }

        void LoadJson(JsonInputArchive& archive) override;
        void SaveJson(JsonOutputArchive& archive) const override;
        void GenerateHLSL(const std::vector<std::string>& inputVars,
                          std::vector<std::string>& outputVars,
                          SGCodeGenContext& ctx) const override;
    private:
        std::string paramName;
        float       defaultValue = 0.0f;
    };

    // VectorParam – an exposed vector (float4) material parameter
    class SGVectorParamNode : public SGNode {
    public:
        explicit SGVectorParamNode(const std::string& paramName = "VecParam");
        ~SGVectorParamNode() override = default;

        std::string GetTypeName() const override    { return "VectorParam"; }
        std::string GetDisplayName() const override { return "Vector Parameter"; }

        void SetParamName(const std::string& n);
        const std::string& GetParamName() const { return paramName; }

        void LoadJson(JsonInputArchive& archive) override;
        void SaveJson(JsonOutputArchive& archive) const override;
        void GenerateHLSL(const std::vector<std::string>& inputVars,
                          std::vector<std::string>& outputVars,
                          SGCodeGenContext& ctx) const override;
    private:
        std::string paramName;
        std::array<float, 4> defaultValue = {0.f, 0.f, 0.f, 1.f};
    };

    // TextureParam – an exposed Texture2D material parameter
    class SGTextureParamNode : public SGNode {
    public:
        explicit SGTextureParamNode(const std::string& paramName = "Texture");
        ~SGTextureParamNode() override = default;

        std::string GetTypeName() const override    { return "TextureParam"; }
        std::string GetDisplayName() const override { return "Texture Parameter"; }

        void SetParamName(const std::string& n);
        const std::string& GetParamName() const { return paramName; }

        void LoadJson(JsonInputArchive& archive) override;
        void SaveJson(JsonOutputArchive& archive) const override;
        void GenerateHLSL(const std::vector<std::string>& inputVars,
                          std::vector<std::string>& outputVars,
                          SGCodeGenContext& ctx) const override;
    private:
        std::string paramName;
    };

    // TextureSample – samples a Texture2D with UV coordinates
    // Input 0: Texture2D, Input 1: SamplerState, Input 2: UV (float2)
    // Output 0: RGBA (float4), Output 1: RGB (float3), Output 2: R (float)
    class SGTextureSampleNode : public SGNode {
    public:
        SGTextureSampleNode();
        ~SGTextureSampleNode() override = default;

        std::string GetTypeName() const override    { return "TextureSample"; }
        std::string GetDisplayName() const override { return "Texture Sample"; }
        void GenerateHLSL(const std::vector<std::string>& inputVars,
                          std::vector<std::string>& outputVars,
                          SGCodeGenContext& ctx) const override;
    };

} // namespace sky::sg

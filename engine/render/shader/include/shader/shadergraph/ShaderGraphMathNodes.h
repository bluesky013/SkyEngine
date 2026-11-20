//
// Created by blues on 2026/3/10.
//

#pragma once

#include <shader/shadergraph/ShaderGraphNode.h>

namespace sky::sg {

    // ---- Binary operator base ----
    // All binary nodes follow the pattern: two same-typed inputs → one output of the same type.
    class SGBinaryMathNode : public SGNode {
    public:
        explicit SGBinaryMathNode(SGDataType dataType = SGDataType::FLOAT3);
        ~SGBinaryMathNode() override = default;

        void SetDataType(SGDataType dataType);
        void LoadJson(JsonInputArchive& archive) override;
        void SaveJson(JsonOutputArchive& archive) const override;

    protected:
        void GenerateBinary(const std::vector<std::string>& inputVars,
                            std::vector<std::string>&       outputVars,
                            SGCodeGenContext&                ctx,
                            const char*                      op) const;

        SGDataType dataType;
    };

    // Add: Output = A + B
    class SGAddNode : public SGBinaryMathNode {
    public:
        explicit SGAddNode(SGDataType dataType = SGDataType::FLOAT3);
        ~SGAddNode() override = default;

        std::string GetTypeName() const override    { return "Add"; }
        std::string GetDisplayName() const override { return "Add"; }
        void GenerateHLSL(const std::vector<std::string>& inputVars,
                          std::vector<std::string>& outputVars,
                          SGCodeGenContext& ctx) const override;
    };

    // Subtract: Output = A - B
    class SGSubtractNode : public SGBinaryMathNode {
    public:
        explicit SGSubtractNode(SGDataType dataType = SGDataType::FLOAT3);
        ~SGSubtractNode() override = default;

        std::string GetTypeName() const override    { return "Subtract"; }
        std::string GetDisplayName() const override { return "Subtract"; }
        void GenerateHLSL(const std::vector<std::string>& inputVars,
                          std::vector<std::string>& outputVars,
                          SGCodeGenContext& ctx) const override;
    };

    // Multiply: Output = A * B
    class SGMultiplyNode : public SGBinaryMathNode {
    public:
        explicit SGMultiplyNode(SGDataType dataType = SGDataType::FLOAT3);
        ~SGMultiplyNode() override = default;

        std::string GetTypeName() const override    { return "Multiply"; }
        std::string GetDisplayName() const override { return "Multiply"; }
        void GenerateHLSL(const std::vector<std::string>& inputVars,
                          std::vector<std::string>& outputVars,
                          SGCodeGenContext& ctx) const override;
    };

    // Divide: Output = A / B
    class SGDivideNode : public SGBinaryMathNode {
    public:
        explicit SGDivideNode(SGDataType dataType = SGDataType::FLOAT3);
        ~SGDivideNode() override = default;

        std::string GetTypeName() const override    { return "Divide"; }
        std::string GetDisplayName() const override { return "Divide"; }
        void GenerateHLSL(const std::vector<std::string>& inputVars,
                          std::vector<std::string>& outputVars,
                          SGCodeGenContext& ctx) const override;
    };

    // Lerp: Output = lerp(A, B, Alpha)
    class SGLerpNode : public SGNode {
    public:
        explicit SGLerpNode(SGDataType dataType = SGDataType::FLOAT3);
        ~SGLerpNode() override = default;

        std::string GetTypeName() const override    { return "Lerp"; }
        std::string GetDisplayName() const override { return "Lerp"; }

        void SetDataType(SGDataType dataType);
        void LoadJson(JsonInputArchive& archive) override;
        void SaveJson(JsonOutputArchive& archive) const override;
        void GenerateHLSL(const std::vector<std::string>& inputVars,
                          std::vector<std::string>& outputVars,
                          SGCodeGenContext& ctx) const override;
    private:
        SGDataType dataType;
    };

    // Clamp: Output = clamp(Value, Min, Max)
    class SGClampNode : public SGNode {
    public:
        explicit SGClampNode(SGDataType dataType = SGDataType::FLOAT);
        ~SGClampNode() override = default;

        std::string GetTypeName() const override    { return "Clamp"; }
        std::string GetDisplayName() const override { return "Clamp"; }

        void SetDataType(SGDataType dataType);
        void LoadJson(JsonInputArchive& archive) override;
        void SaveJson(JsonOutputArchive& archive) const override;
        void GenerateHLSL(const std::vector<std::string>& inputVars,
                          std::vector<std::string>& outputVars,
                          SGCodeGenContext& ctx) const override;
    private:
        SGDataType dataType;
    };

    // Saturate: Output = saturate(Value)
    class SGSaturateNode : public SGNode {
    public:
        explicit SGSaturateNode(SGDataType dataType = SGDataType::FLOAT);
        ~SGSaturateNode() override = default;

        std::string GetTypeName() const override    { return "Saturate"; }
        std::string GetDisplayName() const override { return "Saturate"; }

        void SetDataType(SGDataType dataType);
        void LoadJson(JsonInputArchive& archive) override;
        void SaveJson(JsonOutputArchive& archive) const override;
        void GenerateHLSL(const std::vector<std::string>& inputVars,
                          std::vector<std::string>& outputVars,
                          SGCodeGenContext& ctx) const override;
    private:
        SGDataType dataType;
    };

    // Abs: Output = abs(Value)
    class SGAbsNode : public SGNode {
    public:
        explicit SGAbsNode(SGDataType dataType = SGDataType::FLOAT);
        ~SGAbsNode() override = default;

        std::string GetTypeName() const override    { return "Abs"; }
        std::string GetDisplayName() const override { return "Abs"; }

        void SetDataType(SGDataType dataType);
        void LoadJson(JsonInputArchive& archive) override;
        void SaveJson(JsonOutputArchive& archive) const override;
        void GenerateHLSL(const std::vector<std::string>& inputVars,
                          std::vector<std::string>& outputVars,
                          SGCodeGenContext& ctx) const override;
    private:
        SGDataType dataType;
    };

    // Power: Output = pow(Base, Exp) – both float
    class SGPowerNode : public SGNode {
    public:
        SGPowerNode();
        ~SGPowerNode() override = default;

        std::string GetTypeName() const override    { return "Power"; }
        std::string GetDisplayName() const override { return "Power"; }
        void GenerateHLSL(const std::vector<std::string>& inputVars,
                          std::vector<std::string>& outputVars,
                          SGCodeGenContext& ctx) const override;
    };

    // Dot: Output = dot(A, B) → float
    class SGDotNode : public SGNode {
    public:
        explicit SGDotNode(SGDataType dataType = SGDataType::FLOAT3);
        ~SGDotNode() override = default;

        std::string GetTypeName() const override    { return "Dot"; }
        std::string GetDisplayName() const override { return "Dot"; }

        void SetDataType(SGDataType dataType);
        void LoadJson(JsonInputArchive& archive) override;
        void SaveJson(JsonOutputArchive& archive) const override;
        void GenerateHLSL(const std::vector<std::string>& inputVars,
                          std::vector<std::string>& outputVars,
                          SGCodeGenContext& ctx) const override;
    private:
        SGDataType dataType;
    };

    // Cross: Output = cross(A, B) → float3
    class SGCrossNode : public SGNode {
    public:
        SGCrossNode();
        ~SGCrossNode() override = default;

        std::string GetTypeName() const override    { return "Cross"; }
        std::string GetDisplayName() const override { return "Cross"; }
        void GenerateHLSL(const std::vector<std::string>& inputVars,
                          std::vector<std::string>& outputVars,
                          SGCodeGenContext& ctx) const override;
    };

    // Normalize: Output = normalize(V) → float3
    class SGNormalizeNode : public SGNode {
    public:
        SGNormalizeNode();
        ~SGNormalizeNode() override = default;

        std::string GetTypeName() const override    { return "Normalize"; }
        std::string GetDisplayName() const override { return "Normalize"; }
        void GenerateHLSL(const std::vector<std::string>& inputVars,
                          std::vector<std::string>& outputVars,
                          SGCodeGenContext& ctx) const override;
    };

    // ComponentMask: extract selected channels from a vector
    class SGComponentMaskNode : public SGNode {
    public:
        explicit SGComponentMaskNode(SGDataType inputType = SGDataType::FLOAT4);
        ~SGComponentMaskNode() override = default;

        std::string GetTypeName() const override    { return "ComponentMask"; }
        std::string GetDisplayName() const override { return "Component Mask"; }

        void SetChannels(bool r, bool g, bool b, bool a);
        void LoadJson(JsonInputArchive& archive) override;
        void SaveJson(JsonOutputArchive& archive) const override;
        void GenerateHLSL(const std::vector<std::string>& inputVars,
                          std::vector<std::string>& outputVars,
                          SGCodeGenContext& ctx) const override;
    private:
        void UpdateOutputPin();

        bool maskR = true;
        bool maskG = true;
        bool maskB = true;
        bool maskA = false;
        SGDataType inputType;
    };

    // Append: combine two values into a larger vector
    class SGAppendNode : public SGNode {
    public:
        explicit SGAppendNode(SGDataType aType = SGDataType::FLOAT3, SGDataType bType = SGDataType::FLOAT);
        ~SGAppendNode() override = default;

        std::string GetTypeName() const override    { return "Append"; }
        std::string GetDisplayName() const override { return "Append"; }

        void LoadJson(JsonInputArchive& archive) override;
        void SaveJson(JsonOutputArchive& archive) const override;
        void GenerateHLSL(const std::vector<std::string>& inputVars,
                          std::vector<std::string>& outputVars,
                          SGCodeGenContext& ctx) const override;
    private:
        SGDataType aType;
        SGDataType bType;
    };

} // namespace sky::sg

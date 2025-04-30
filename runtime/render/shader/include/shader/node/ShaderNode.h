//
// Created by blues on 2025/3/28.
//

#pragma once

#include <core/name/Name.h>
#include <core/template/ReferenceObject.h>
#include <shader/node/ShaderDataType.h>
#include <unordered_map>

namespace sky::sl {

    class NodeBase : public RefObject {
    public:
        explicit NodeBase() = default;
        ~NodeBase() override = default;
    };

    class TypeNode;
    class Expression;
    class Statement;
    class Declation;

#pragma region Type

    class TypeNode : public NodeBase {
    public:
        TypeNode() = default;
        ~TypeNode() override = default;
    };

    class ScalarType : public TypeNode {
    public:
        ScalarType() = default;
        ~ScalarType() override = default;
    };

    class VectorType : public TypeNode {
    public:
        VectorType() = default;
        ~VectorType() override = default;

    private:
        uint32_t dimension;
    };

    class MatrixType : public TypeNode {
    public:
        MatrixType() = default;
        ~MatrixType() override = default;
    private:
        uint32_t row;
        uint32_t column;
    };

    class StructureType : public TypeNode {
    public:
        StructureType() = default;
        ~StructureType() override = default;

    private:
        std::string name;
    };

    class ShaderResourceType : public TypeNode {
    public:
        ShaderResourceType() = default;
        ~ShaderResourceType() override = default;

    protected:
        std::string name;
    };

    class ConstantBufferNode : public ShaderResourceType {
    public:
        ConstantBufferNode() = default;
        ~ConstantBufferNode() override = default;
    };

    class TextureNode : public ShaderResourceType {
    public:
        TextureNode() = default;
        ~TextureNode() override = default;
    };

    class SamplerNode : public ShaderResourceType {
    public:
        SamplerNode() = default;
        ~SamplerNode() override = default;
    };

#pragma endregion

#pragma region Expression

    class Expression : public NodeBase {
    public:
        Expression() = default;
        ~Expression() override = default;
    };

#pragma endregion


} // namespace sky::sl
//
// Created on 2026/10/23.
//

#pragma once

#include <aurora/rhi/ShaderReflection.h>
#include <core/math/Matrix4.h>
#include <core/math/Vector2.h>
#include <core/math/Vector4.h>
#include <core/name/Name.h>
#include <cstddef>
#include <cstring>
#include <map>
#include <memory>
#include <set>
#include <span>
#include <type_traits>

namespace sky::aurora {
    class Shader;
    enum class ShaderParameterWriteStatus : uint8_t { SUCCESS, INACTIVE, FAILED };
    struct ShaderParameterWriteResult {
        ShaderParameterWriteStatus status = ShaderParameterWriteStatus::FAILED;
        explicit                   operator bool() const noexcept
        {
            return status != ShaderParameterWriteStatus::FAILED;
        }
    };
    enum class ShaderDataType : uint8_t { UNKNOWN, FLOAT, INT, UINT, FLOAT2, FLOAT4, MAT4, FLOAT4_ARRAY };
    struct ShaderDataValue {
        ShaderDataType         type         = ShaderDataType::UNKNOWN;
        uint32_t               count        = 0;
        ShaderMatrixLayout     matrixLayout = ShaderMatrixLayout::COLUMN_MAJOR;
        std::vector<std::byte> bytes;
    };
    template <class T>
    constexpr ShaderDataType ShaderDataTypeOf()
    {
        if constexpr (std::is_same_v<T, float>)
            return ShaderDataType::FLOAT;
        else if constexpr (std::is_same_v<T, int32_t>)
            return ShaderDataType::INT;
        else if constexpr (std::is_same_v<T, uint32_t>)
            return ShaderDataType::UINT;
        else if constexpr (std::is_same_v<T, Vector2>)
            return ShaderDataType::FLOAT2;
        else if constexpr (std::is_same_v<T, Vector4>)
            return ShaderDataType::FLOAT4;
        else if constexpr (std::is_same_v<T, Matrix4>)
            return ShaderDataType::MAT4;
        else
            return ShaderDataType::UNKNOWN;
    }
    template <class T>
    ShaderDataValue MakeShaderData(const T &value)
    {
        static_assert(ShaderDataTypeOf<T>() != ShaderDataType::UNKNOWN, "Unsupported shader data type");
        ShaderDataValue result{.type = ShaderDataTypeOf<T>()};
        result.bytes.resize(sizeof(T));
        std::memcpy(result.bytes.data(), &value, sizeof(T));
        return result;
    }
    inline ShaderDataValue MakeShaderData(std::span<const Vector4> value)
    {
        ShaderDataValue result{.type = ShaderDataType::FLOAT4_ARRAY, .count = static_cast<uint32_t>(value.size())};
        const auto      bytes = std::as_bytes(value);
        result.bytes.assign(bytes.begin(), bytes.end());
        return result;
    }
    struct ShaderCpuSchema {
        uint32_t                       size = 0, alignment = 1;
        std::vector<ShaderBlockMember> fields;
    };
    template <class T>
    struct ShaderUniformSlot : ShaderResourceSlot<ShaderResourceType::UNIFORM_BUFFER> {};
    struct UniformCopySegment {
        uint32_t source = 0, target = 0, size = 0;
    };
    struct UniformUploadPlan {
        static constexpr uint32_t       kProtocolVersion = 1;
        uint64_t                        key              = 0;
        uint32_t                        sourceSize = 0, targetSize = 0;
        bool                            wholeCopy = false;
        std::vector<UniformCopySegment> segments;
    };
    ShaderDataType ShaderMemberDataType(const ShaderBlockMember &member);
    bool           ValidateShaderData(const ShaderDataValue &value, std::string &error);
    uint64_t       ShaderCpuSchemaFingerprint(const ShaderCpuSchema &schema);
    bool           BuildUniformUploadPlan(
                  const ShaderCpuSchema &cpu, const ShaderBlockLayout &target, uint64_t artifactId, UniformUploadPlan &out, std::string &error);
    bool EncodeShaderUniform(const UniformUploadPlan &plan, std::span<const std::byte> source, std::vector<std::byte> &out, std::string &error);
    bool PackShaderData(const ShaderBlockLayout                                 &target,
                        std::span<const std::pair<std::string, ShaderDataValue>> values,
                        std::vector<std::byte>                                  &out,
                        std::string                                             &error);
    bool BuildShaderDataSchema(std::span<const std::pair<std::string, ShaderDataValue>> values,
                               ShaderCpuSchema                                         &schema,
                               std::vector<std::byte>                                  &bytes,
                               std::string                                             &error);

    enum class ShaderParameterKind : uint8_t { RESOURCE, FIELD };
    enum class ShaderParameterLookup : uint8_t { READY, INACTIVE, UNKNOWN, AMBIGUOUS, STALE };
    struct ShaderParameterEntry {
        std::string         name;
        uint32_t            set = 0, binding = 0;
        ShaderParameterKind kind         = ShaderParameterKind::RESOURCE;
        ShaderResourceType  resourceType = ShaderResourceType::SAMPLER;
        ShaderDataType      dataType     = ShaderDataType::UNKNOWN;
        uint32_t            arrayCount   = 0;
        bool                active       = false;
        std::string         field;
    };
    struct ShaderParameterHandle {
        uint64_t generation = 0;
        uint32_t index = UINT32_MAX, set = UINT32_MAX;
    };
    struct ShaderParameterAlias {
        std::string alias, target;
    };
    struct ShaderParameterTable {
        uint64_t                          generation = 0;
        std::vector<ShaderParameterEntry> entries;
        std::vector<ShaderParameterAlias> aliases;
        ShaderParameterLookup             Resolve(Name name, uint32_t set, ShaderParameterHandle &out) const;
        ShaderParameterLookup             Resolve(ShaderParameterHandle handle, const ShaderParameterEntry *&out) const;
    };
    struct ShaderParameterResourceWrite {
        uint32_t           binding = 0, element = 0, token = 0;
        ShaderResourceType type = ShaderResourceType::SAMPLER;
    };
    struct ShaderParameterUniformWrite {
        uint32_t               binding   = 0;
        ShaderResourceType     type      = ShaderResourceType::UNIFORM_BUFFER;
        uint32_t               alignment = 1;
        std::vector<std::byte> bytes;
    };
    struct ShaderParameterTransactionState {
        std::map<uint32_t, std::map<std::string, ShaderDataValue>>            fields;
        std::map<std::pair<uint32_t, uint32_t>, ShaderParameterResourceWrite> resources;
    };
    struct ShaderParameterPacket {
        uint64_t                                  generation = 0;
        uint32_t                                  set        = 0;
        std::vector<ShaderParameterUniformWrite>  uniforms;
        std::vector<ShaderParameterResourceWrite> resources;
    };
    // The shader and adapter token registry outlive the transaction and its materialization.
    class ShaderParameterTransaction {
    public:
        ShaderParameterTransaction(const Shader &shader, uint32_t set, const ShaderParameterTransactionState &initial = {});
        ShaderParameterWriteResult             WriteData(Name name, const ShaderDataValue &value);
        ShaderParameterWriteResult             WriteData(ShaderParameterHandle handle, const ShaderDataValue &value);
        ShaderParameterWriteResult             WriteBlock(Name name, const ShaderCpuSchema &schema, std::span<const std::byte> bytes);
        ShaderParameterWriteResult             WriteResource(Name name, ShaderResourceType type, uint32_t token, uint32_t arrayElement = 0);
        bool                                   Complete();
        bool                                   Finalize(std::shared_ptr<const ShaderParameterPacket> &out);
        const ShaderParameterTransactionState &GetState() const noexcept
        {
            return staged;
        }
        const std::string &GetError() const noexcept
        {
            return error;
        }
        bool Fail(std::string message);
        void Cancel();

    private:
        bool                                       Open() const noexcept;
        bool                                       Resolve(Name name, ShaderParameterHandle &handle);
        const ShaderParameterEntry                *Resource(Name name);
        bool                                       Touch(uint32_t binding, const std::string &field, bool whole);
        const Shader                              &shader;
        uint32_t                                   set = 0;
        ShaderParameterTransactionState            staged;
        std::set<std::pair<uint32_t, std::string>> touched;
        std::set<uint32_t>                         wholeBlocks, touchedBindings;
        std::string                                error;
        bool                                       ended = false;
    };

    bool BuildShaderParameterTable(const ShaderReflection               &reflection,
                                   uint64_t                              generation,
                                   std::span<const ShaderParameterAlias> aliases,
                                   ShaderParameterTable                 &out,
                                   std::string                          &error);
} // namespace sky::aurora

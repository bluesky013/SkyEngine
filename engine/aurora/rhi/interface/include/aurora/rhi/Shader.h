//
// Created by Zach Lee on 2026/3/30.
//

#pragma once

#include <aurora/rhi/Core.h>
#include <aurora/rhi/ShaderArtifact.h>
#include <aurora/rhi/ShaderParameters.h>
#include <core/archive/BinaryData.h>
#include <core/template/ReferenceObject.h>

#include <map>
#include <mutex>
#include <string>
#include <tuple>

namespace sky::aurora {

    class Device;

    struct ShaderDataProvider : RefObject {
        ShaderDataProvider()           = default;
        ~ShaderDataProvider() override = default;
        virtual BinaryData *GetBinaryData() const noexcept
        {
            return nullptr;
        }
    };

    struct ShaderBinaryProvider : ShaderDataProvider {
        BinaryDataPtr binaryData;
        BinaryData   *GetBinaryData() const noexcept override
        {
            return binaryData.Get();
        }
    };

    class ShaderFunction : public RefObject {
    public:
        virtual const Device *GetDevice() const noexcept
        {
            return nullptr;
        }
        struct Descriptor {
            ShaderStageFlagBit             stage;
            CounterPtr<ShaderDataProvider> data;
            std::string                    entry;
            ShaderStageArtifactPtr         artifact;
        };
        ShaderFunction()           = default;
        ~ShaderFunction() override = default;
        const ShaderStageArtifactPtr &GetArtifact() const noexcept
        {
            return artifact;
        }
        const std::string &GetNativeEntry() const noexcept
        {
            return artifact->identity.nativeEntry;
        }

    protected:
        bool                   InitArtifact(const Descriptor &desc, API backend, std::string &error);
        ShaderStageArtifactPtr artifact;
    };
    using ShaderFunctionPtr = CounterPtr<ShaderFunction>;

    struct ShaderSpecializationEntry {
        uint32_t id    = 0; // specialization constant id
        uint32_t value = 0;
        ShaderStageFlagBit stage = ShaderStageFlagBit::FS;
        ShaderVariantType type = ShaderVariantType::UINT32;
        std::string key;
        bool operator==(const ShaderSpecializationEntry &) const = default;
    };
    struct ShaderSpecialization {
        std::vector<ShaderSpecializationEntry> entries;
    };

    struct ShaderBindingPolicy {
        uint32_t           set = 0, binding = 0;
        ShaderResourceType type = ShaderResourceType::UNIFORM_BUFFER_DYNAMIC;
    };

    class Shader : public RefObject {
    public:
        // Unknown owners cannot enter backend-specific native paths.
        virtual const Device *GetDevice() const noexcept
        {
            return nullptr;
        }
        // Whole-program layout used by graph analysis; backends return their frozen reflection.
        virtual const ShaderReflection &GetReflection() const
        {
            static const ShaderReflection empty;
            return empty;
        }
        const ShaderParameterTable &GetParameterTable() const noexcept
        {
            return parameters;
        }
        std::shared_ptr<const UniformUploadPlan> GetUniformPlan(uint32_t set, uint32_t binding, const ShaderCpuSchema &cpu, std::string &error) const;
        bool                                     PackUniformData(uint32_t                                                 set,
                                                                 uint32_t                                                 binding,
                                                                 std::span<const std::pair<std::string, ShaderDataValue>> values,
                                                                 std::vector<std::byte>                                  &bytes,
                                                                 std::string                                             &error) const;

        struct Descriptor {
            union {
                struct {
                    ShaderFunction *vs;
                    ShaderFunction *ps;
                };
                struct {
                    ShaderFunction *cs;
                };
            };
            const ShaderReflection           *reflection     = nullptr; // Optional assertion of the layout derived from the stage artifacts.
            const ShaderSpecialization       *specialization = nullptr; // spec constant values
            std::vector<ShaderBindingPolicy>  bindingPolicies;
            std::vector<ShaderParameterAlias> parameterAliases;
        };

        Shader()           = default;
        ~Shader() override = default;

    protected:
        bool                 InitParameterTable(const Descriptor &desc, std::string &error);
        ShaderSpecialization specialization;
        ShaderParameterTable parameters;
        mutable std::mutex   uniformPlanMutex;
        mutable std::map<std::tuple<uint32_t, uint32_t, uint64_t>, std::shared_ptr<const UniformUploadPlan>> uniformPlans;
    };
    using ShaderPtr = CounterPtr<Shader>;

    bool BuildShaderProgramReflection(const Shader::Descriptor &desc, const Device &device, ShaderReflection &reflection, std::string &error);

} // namespace sky::aurora

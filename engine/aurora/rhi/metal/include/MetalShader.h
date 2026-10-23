//
// Created on 2026/04/02.
//

#pragma once

#include <aurora/rhi/Shader.h>

#include <array>
#include <unordered_map>
#include <vector>

namespace sky::aurora {

    class MetalDevice;

    struct MetalMSLSetSlots {
        std::unordered_map<uint32_t, uint32_t> buffer;
        std::unordered_map<uint32_t, uint32_t> texture;
        std::unordered_map<uint32_t, uint32_t> sampler;
    };

    struct MetalResourceSetPlan {
        std::array<MetalMSLSetSlots, 3> stages;
        std::vector<uint32_t>           dynamicBindings;
    };

    class MetalShaderFunction : public ShaderFunction {
    public:
        explicit MetalShaderFunction(MetalDevice &dev);
        ~MetalShaderFunction() override;

        bool Init(const Descriptor &desc);

        const Device *GetDevice() const noexcept override;
        void         *GetNativeHandle() const
        {
            return function;
        }
        ShaderStageFlagBit GetStage() const
        {
            return stage;
        }

    private:
        friend class MetalShader;
        bool InitSpecialization(const MetalShaderFunction &source, const ShaderSpecialization &values);

        MetalDevice       &device;
        void              *library  = nullptr;
        void              *function = nullptr;
        ShaderStageFlagBit stage    = ShaderStageFlagBit::VS;
    };

    class MetalShader : public Shader {
    public:
        explicit MetalShader(MetalDevice &dev);
        ~MetalShader() override = default;

        bool Init(const Descriptor &desc);

        const Device        *GetDevice() const noexcept override;
        MetalShaderFunction *GetVertexFunction() const
        {
            return vertexFunction.Get();
        }
        MetalShaderFunction *GetFragmentFunction() const
        {
            return fragmentFunction.Get();
        }
        MetalShaderFunction *GetComputeFunction() const
        {
            return computeFunction.Get();
        }

        const ShaderReflection &GetReflection() const override
        {
            return reflection;
        }
        const MetalResourceSetPlan *GetResourceSetPlan(uint32_t set) const
        {
            const auto found = resourceSetPlans.find(set);
            return found == resourceSetPlans.end() ? nullptr : &found->second;
        }
        // slang MSL lowers push constants to a plain constant buffer at the
        // highest [[buffer(N)]] slot (declare-last convention)
        uint32_t GetPushConstantSlot() const
        {
            return pushConstantSlot;
        }
        // Vertex-stage extent protects the reserved vertex input slots; compute uses its own extent.
        uint32_t GetBufferSlotCount() const
        {
            return bufferSlotCount;
        }
        // size of the push constant block (max offset+size over reflected ranges)
        uint32_t GetPushConstantSize() const
        {
            return pushConstantSize;
        }

    private:
        bool SpecializeFunctions();

        MetalDevice                                       &device;
        CounterPtr<MetalShaderFunction>                    vertexFunction;
        CounterPtr<MetalShaderFunction>                    fragmentFunction;
        CounterPtr<MetalShaderFunction>                    computeFunction;
        ShaderReflection                                   reflection;
        std::unordered_map<uint32_t, MetalResourceSetPlan> resourceSetPlans;
        uint32_t                                           pushConstantSlot = 0;
        uint32_t                                           bufferSlotCount  = 0;
        uint32_t                                           pushConstantSize = 0;
    };

} // namespace sky::aurora

//
// Created on 2026/04/02.
//

#pragma once

#include <aurora/rhi/Shader.h>

namespace sky::aurora {

    class MetalDevice;

    class MetalShaderFunction : public ShaderFunction {
    public:
        explicit MetalShaderFunction(MetalDevice &dev);
        ~MetalShaderFunction() override;

        bool Init(const Descriptor &desc);

        void *GetNativeHandle() const { return function; }
        ShaderStageFlagBit GetStage() const { return stage; }

    private:
        MetalDevice        &device;
        void               *library  = nullptr;
        void               *function = nullptr;
        ShaderStageFlagBit  stage    = ShaderStageFlagBit::VS;
    };

    class MetalShader : public Shader {
    public:
        explicit MetalShader(MetalDevice &dev);
        ~MetalShader() override = default;

        bool Init(const Descriptor &desc);

        MetalShaderFunction *GetVertexFunction() const { return vertexFunction.Get(); }
        MetalShaderFunction *GetFragmentFunction() const { return fragmentFunction.Get(); }
        MetalShaderFunction *GetComputeFunction() const { return computeFunction.Get(); }

        const ShaderReflection &GetReflection() const { return reflection; }
        // slang MSL lowers push constants to a plain constant buffer at the
        // highest [[buffer(N)]] slot (declare-last convention)
        uint32_t GetPushConstantSlot() const { return pushConstantSlot; }
        // one past the highest [[buffer(N)]] slot used by the shader (incl.
        // push constants); must stay below METAL_VERTEX_BUFFER_SLOT_BASE
        uint32_t GetBufferSlotCount() const { return bufferSlotCount; }
        // size of the push constant block (max offset+size over reflected ranges)
        uint32_t GetPushConstantSize() const { return pushConstantSize; }

    private:
        MetalDevice                    &device;
        CounterPtr<MetalShaderFunction> vertexFunction;
        CounterPtr<MetalShaderFunction> fragmentFunction;
        CounterPtr<MetalShaderFunction> computeFunction;
        ShaderReflection                reflection;
        uint32_t                        pushConstantSlot = 0;
        uint32_t                        bufferSlotCount  = 0;
        uint32_t                        pushConstantSize = 0;
    };

} // namespace sky::aurora
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

    private:
        MetalDevice                    &device;
        CounterPtr<MetalShaderFunction> vertexFunction;
        CounterPtr<MetalShaderFunction> fragmentFunction;
        CounterPtr<MetalShaderFunction> computeFunction;
    };

} // namespace sky::aurora
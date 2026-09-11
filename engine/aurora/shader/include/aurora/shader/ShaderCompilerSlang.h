//
// Aurora shader compiler: Slang backend (SPIRV / MSL direct emission).
//

#pragma once

#include <aurora/shader/ShaderReflection.h>
#include <aurora/rhi/Core.h>

#include <string>
#include <vector>

namespace sky::aurora {

    enum class ShaderTarget : uint32_t {
        SPIRV = 0,
        MSL,
        DXIL,
    };

    struct ShaderCompileDesc {
        std::string        source;
        std::string        entry;
        ShaderStageFlagBit stage;
        ShaderTarget       target = ShaderTarget::SPIRV;
    };

    struct ShaderCompileResult {
        std::vector<uint32_t> data;   // SPIRV words; MSL text packed into words
        ShaderReflection      reflection;
        std::string           errorInfo;
    };

    class ShaderCompilerSlang {
    public:
        ShaderCompilerSlang() = default;
        ~ShaderCompilerSlang() = default;

        ShaderCompilerSlang(const ShaderCompilerSlang &) = delete;
        ShaderCompilerSlang &operator=(const ShaderCompilerSlang &) = delete;

        bool Compile(const ShaderCompileDesc &desc, ShaderCompileResult &result);

    private:
        bool InitGlobalSession();
    };

} // namespace sky::aurora

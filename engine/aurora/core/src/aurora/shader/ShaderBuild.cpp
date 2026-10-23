//
// ShaderBuild implementation.
//

#include <aurora/shader/ShaderBuild.h>

#include <aurora/rhi/Device.h>

#include <core/archive/BinaryData.h>

#include <cstring>

namespace sky::aurora {

    ShaderFunctionPtr
    CreateShaderFunctionFromResult(Device &device, const ShaderCompileResult &result, ShaderStageFlagBit stage, const std::string &entry)
    {
        if (result.identity.stage != stage || result.identity.sourceEntry != entry)
            return {};
        ShaderFunction::Descriptor desc{};
        desc.stage    = stage;
        desc.entry    = result.identity.nativeEntry;
        desc.artifact = std::make_shared<const ShaderStageArtifact>(result);
        return ShaderFunctionPtr(device.CreateShaderFunction(desc));
    }

} // namespace sky::aurora

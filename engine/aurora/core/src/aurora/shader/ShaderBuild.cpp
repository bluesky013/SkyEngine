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
        auto *provider       = new ShaderBinaryProvider();
        provider->binaryData = CounterPtr<BinaryData>(new BinaryData(static_cast<uint32_t>(result.data.size() * sizeof(uint32_t))));
        if (!result.data.empty()) {
            std::memcpy(provider->binaryData->Data(), result.data.data(), result.data.size() * sizeof(uint32_t));
        }

        ShaderFunction::Descriptor desc;
        desc.stage = stage;
        desc.data  = provider;
        desc.entry = entry;
        return ShaderFunctionPtr(device.CreateShaderFunction(desc));
    }

} // namespace sky::aurora

//
// Created by blues on 2024/6/10.
//

#include <render/light/LightFeatureProcessor.h>

namespace sky {

    void LightFeatureProcessor::Tick(float time)
    {
//        GatherLightInfo();
    }

    void LightFeatureProcessor::AddLight(Light *light)
    {
        lights.emplace_back(light);
    }

    void LightFeatureProcessor::RemoveLight(Light *light)
    {
        lights.erase(std::remove_if(lights.begin(), lights.end(), [light](const LightPtr &v) {
            return light == v.get();
        }), lights.end());
    }

    void LightFeatureProcessor::GatherLightInfo()
    {
        auto lightCount = static_cast<uint32_t>(lights.size());
        if (lightCount == 0) {
            return;
        }

        auto lightBufferStride = static_cast<uint32_t>(sizeof(LightInfo));
        uint32_t lightBufferSize = lightCount * lightBufferStride;

        if (!lightData || lightData->GetSize() < lightBufferStride) {
            lightData = new Buffer();
            lightData->Init(lightBufferSize,
                rhi::BufferUsageFlagBit::STORAGE | rhi::BufferUsageFlagBit::TRANSFER_DST,
                rhi::MemoryType::GPU_ONLY);

            stagingBuffer = new Buffer();
            stagingBuffer->Init(lightBufferSize,
                rhi::BufferUsageFlagBit::TRANSFER_SRC,
                rhi::MemoryType::CPU_TO_GPU);
        }

        auto *lightInfos = reinterpret_cast<LightInfo*>(stagingBuffer->GetRHIBuffer()->Map());

        for (uint32_t i = 0; i < lightCount; ++i) {
            auto &lightInfo = lightInfos[i];
            auto &light = lights[i];
            light->Collect(lightInfo);
        }

        stagingBuffer->GetRHIBuffer()->UnMap();
    }

} // namespace sky
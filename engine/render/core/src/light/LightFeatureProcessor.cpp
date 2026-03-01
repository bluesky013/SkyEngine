//
// Created by blues on 2024/6/10.
//

#include <render/light/LightFeatureProcessor.h>
#include <core/math/Color.h>
#include <core/math/MathUtil.h>
#include <core/math/Transform.h>
namespace sky {

    ColorRGB KelvinToColorRGB(float kelvin)
    {
        kelvin = std::clamp(kelvin, 1000.0f, 40000.0f);
        float temp = kelvin / 100.f;

        float red, green, blue;
        if (temp <= 66.f) {
            red = 255.f;
        } else {
            red = temp - 60.0f;
            red = 329.698727446f * pow(red, -0.1332047592f);
            red = std::clamp(red, 0.0f, 255.0f);
        }

        if (temp <= 66.f) {
            green = temp;
            green = 99.4708025861f * log(green) - 161.1195681661f;
        } else {
            green = temp - 60.f;
            green = 288.1221695283f * pow(green, -0.0755148492f);
        }
        green = std::clamp(green, 0.0f, 255.0f);

        if (temp >= 66.0f) {
            blue = 255.0f;
        } else if (temp <= 19.0f) {
            blue = 0.0;
        } else {
            blue = temp - 10.0f;
            blue = 138.5177312231f * log(blue) - 305.0447927307f;
            blue = std::clamp(blue, 0.0f, 255.0f);
        }

        return {red, green, blue};
    }

    float CalKelvin(float t)
    {
        t = fmod(t, 24.0f);
        if (t < 5.0f || t >= 19.0f) {
            return 3000.0f;
        } else if (t < 7.0f) {
            float phase = (t - 5.0f) / 2.0f;
            return 3000.0f + 2500.0f * (1.0f - cos(PI * phase)) / 2.0f;
        } else if (t < 17.0f) {
            return 5500.0f;
        }

        float phase = (t - 17.0f) / 2.0f;
        return 5500.0f - 2500.0f * (1.0f - cos(PI * phase)) / 2.0f;
    }

    float CalBrightness(float t)
    {
        t = fmod(t, 24.0f);
        if (t < 5.0f || t >= 19.0f) {
            return 0.1f;
        } else if (t < 7.0f) {
            float phase = (t - 5.0f) / 2.0f;
            return 0.1f + 0.9f * (1.0f - cos(PI * phase)) / 2.0f;
        } else if (t < 17.0f) {
            return 1.0f;
        }
        float phase = (t - 17.0f) / 2.0f;
        return 1.0f - 0.9f * (1.0f - cos(PI * phase)) / 2.0f;
    }

    void TimeOfDay::Tick(float time)
    {
        current += time * 1.f;
        current = static_cast<float>(current > 24 ? static_cast<uint32_t>(current) % 24 : current);

        UpdateTime(current);
    }

    void TimeOfDay::UpdateTime(float time)
    {
        if (light == nullptr) {
            return;
        }

        float kelvin     = CalKelvin(time);
        float brightness = CalBrightness(time);
        ColorRGB color   = KelvinToColorRGB(kelvin);
        color.r /= 255.f;
        color.g /= 255.f;
        color.b /= 255.f;

        float val = time / 24.f * 360.f;
        val = std::clamp(val, 95.f, 265.f);
        float a1 = 90.f  + val;
        float a2 = (-90.f - val) / 180.f * PI;

        radius = 200.f;

        Transform transform;
        transform.translation = Vector3(0.f, radius * sin(a2), radius * cos(a2));
        transform.rotation.FromEulerYZX(Vector3{a1, 0.f, 0});

        light->SetIntensity(brightness * 5.f);
        light->SetColor(color);
        light->SetDirection(transform.rotation * VEC3_NZ);
        light->SetWorldMatrix(transform.ToMatrix());
    }

    void LightFeatureProcessor::Tick(float time)
    {
//        tod.Tick(time);
//        GatherLightInfo();
    }

    MainDirectLight* LightFeatureProcessor::GetOrCreateMainLight()
    {
        if (!mainLight) {
            mainLight = std::make_unique<MainDirectLight>();
        }

        tod.light = mainLight.get();
        return mainLight.get();
    }

    void LightFeatureProcessor::RemoveMainLight()
    {
        mainLight = nullptr;
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
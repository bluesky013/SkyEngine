#pragma once

#include <aurora/adaptor/components/AuroraComponentData.h>
#include <aurora/adaptor/renderer/SceneComponentEvent.h>

namespace sky::aurora {
    class EnvironmentComponent : public SceneComponentAdaptor<EnvironmentData> {
    public:
        COMPONENT_RUNTIME_INFO(EnvironmentComponent)
        static void Reflect(SerializationContext *context);
        void        SetDiffuseIrradiance(const Uuid &value)
        {
            data.diffuseIrradiance = value;
        }
        const Uuid &GetDiffuseIrradiance() const
        {
            return data.diffuseIrradiance;
        }
        void SetSpecularEnvironment(const Uuid &value)
        {
            data.specularEnvironment = value;
        }
        const Uuid &GetSpecularEnvironment() const
        {
            return data.specularEnvironment;
        }
        void SetBrdfIntegration(const Uuid &value)
        {
            data.brdfIntegration = value;
        }
        const Uuid &GetBrdfIntegration() const
        {
            return data.brdfIntegration;
        }
        void SetIntensity(float value)
        {
            data.intensity = value;
        }
        float GetIntensity() const
        {
            return data.intensity;
        }
        void SetExposure(float value)
        {
            data.exposure = value;
        }
        float GetExposure() const
        {
            return data.exposure;
        }
        void SetEnabled(bool value)
        {
            data.enabled = value;
        }
        bool GetEnabled() const
        {
            return data.enabled;
        }
        void SetHdr(bool value)
        {
            data.hdr = value;
        }
        bool GetHdr() const
        {
            return data.hdr;
        }
    };
} // namespace sky::aurora

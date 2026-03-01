//
// Created by Zach Lee on 2023/9/17.
//

#include <core/environment/Singleton.h>
#include <rhi/Device.h>
#include <render/resource/ResourceGroup.h>

namespace sky {

    class ParticleFeature : public Singleton<ParticleFeature> {
    public:
        ParticleFeature() = default;
        ~ParticleFeature() override = default;

        void Init();
    private:
        RDResourceLayoutPtr localLayout;
        rhi::DescriptorSetPoolPtr pool;
    };

} // namespace sky

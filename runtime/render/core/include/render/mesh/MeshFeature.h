//
// Created by Zach Lee on 2023/9/9.
//

#pragma once

#include <core/environment/Singleton.h>
#include <rhi/Device.h>
#include <render/resource/ResourceGroup.h>

namespace sky {

    class MeshFeature : public Singleton<MeshFeature> {
    public:
        MeshFeature() = default;
        ~MeshFeature() override = default;

        void Init();
        RDResourceGroupPtr RequestResourceGroup();
        RDResourceGroupPtr RequestSkinnedResourceGroup();

    private:
        RDResourceLayoutPtr localLayout;
        RDResourceLayoutPtr skinnedLayout;
        rhi::DescriptorSetPoolPtr pool;
    };

} // namespace sky

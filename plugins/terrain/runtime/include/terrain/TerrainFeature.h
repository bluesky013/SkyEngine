//
// Created by blues on 2024/12/12.
//

#pragma once

#include <core/environment/Singleton.h>
#include <rhi/Device.h>
#include <render/resource/ResourceGroup.h>

namespace sky {

    class TerrainFeature : public Singleton<TerrainFeature> {
    public:
        TerrainFeature() = default;
        ~TerrainFeature() override = default;

        void Init();
        RDResourceGroupPtr RequestResourceGroup();

    private:
        RDResourceLayoutPtr localLayout;
        rhi::DescriptorSetPoolPtr pool;
    };

} // namespace sky

//
// Created by Zach Lee on 2022/6/16.
//

#pragma once

#include "VulkanSampleBase.h"

namespace sky {
    class NativeWindow;

    class VulkanMemorySample : public VulkanSampleBase {
    public:
        VulkanMemorySample()  = default;
        ~VulkanMemorySample() = default;

        void Tick(float delta) override;

        void OnStart() override;
        void OnStop() override;

    private:
    };

} // namespace sky
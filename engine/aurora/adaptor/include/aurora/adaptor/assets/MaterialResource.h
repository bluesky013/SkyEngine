//
// Created on 2026/11/15.
//

#pragma once

#include <aurora/adaptor/assets/MaterialAsset.h>
#include <aurora/adaptor/assets/MaterialInstanceAsset.h>

namespace sky::aurora {
    class Device;
    CounterPtr<Material>         CreateMaterialFromAsset(Device *device, const Asset<Material> &asset);
    CounterPtr<MaterialInstance> CreateMaterialInstanceFromAsset(Device *device, const Asset<MaterialInstance> &asset);
} // namespace sky::aurora

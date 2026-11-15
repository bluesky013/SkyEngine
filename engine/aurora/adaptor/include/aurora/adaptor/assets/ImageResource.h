//
// Created on 2026/11/15.
//

#pragma once

#include <aurora/adaptor/assets/ImageAsset.h>

namespace sky::aurora {
    class Device;
    CounterPtr<Texture> CreateTextureFromAsset(Device *device, const Asset<Texture> &asset);
} // namespace sky::aurora

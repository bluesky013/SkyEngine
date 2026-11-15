//
// Created on 2026/11/15.
//

#pragma once

#include <aurora/adaptor/assets/SkinAsset.h>

namespace sky {
    class Skeleton;
}
namespace sky::aurora {
    CounterPtr<Skin> FetchOrCreateSkinByAsset(const std::shared_ptr<Asset<Skin>> &asset, SkinStatus *status = nullptr);
    SkinStatus       MakeSkinTargetLayout(const Skeleton &skeleton, SkinTargetLayout &out);
} // namespace sky::aurora

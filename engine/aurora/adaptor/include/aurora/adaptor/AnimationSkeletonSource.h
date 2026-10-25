#pragma once
#include <animation/IAnimationSkeletonSource.h>
namespace sky::aurora {
    std::unique_ptr<IAnimationSkeletonSource> CreateAnimationSkeletonSource(std::shared_ptr<const void> moduleLease);
}

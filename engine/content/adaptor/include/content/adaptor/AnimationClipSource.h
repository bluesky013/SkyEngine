#pragma once
#include <animation/IAnimationClipSource.h>
namespace sky::content {
    std::unique_ptr<IAnimationClipSource> CreateAnimationClipSource(std::shared_ptr<const void> moduleLease);
}

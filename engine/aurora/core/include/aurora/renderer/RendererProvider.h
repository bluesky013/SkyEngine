#pragma once

#include <aurora/renderer/IRenderer.h>

namespace sky::aurora {

    // Foundation factory for adaptor registration; hosts resolve IRendererProvider.
    std::unique_ptr<IRendererProvider> CreateRendererProvider();

} // namespace sky::aurora

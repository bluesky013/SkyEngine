#pragma once

#include <aurora/renderer/IRenderer.h>

namespace sky::aurora {

    // Private to this plugin; hosts resolve IRendererProvider, never this factory.
    std::unique_ptr<IRendererProvider> CreateRendererProvider();

} // namespace sky::aurora

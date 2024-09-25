//
// Created by blues on 2024/9/3.
//

#pragma once

#include <string_view>

namespace sky {

    static constexpr std::string_view FWD_CL = "ForwardColor";
    static constexpr std::string_view FWD_DS = "ForwardDepthStencil";

    static constexpr std::string_view SHADOW_MAP = "ShadowMap";

    static constexpr std::string_view FWD_MSAA_CL = "ForwardMSAAColor";
    static constexpr std::string_view FWD_MSAA_DS = "ForwardMSAADepthStencil";

    static constexpr std::string_view SWAP_CHAIN = "SwapChain";

} // namespace sky
//
// Created by Copilot on 2026/3/8.
//

#pragma once

#include <rhi/Core.h>

namespace sky {

    // Centralised depth settings that adapt based on whether reverse-z is active.
    // All render passes / pipelines should query these helpers instead of hard-coding
    // clear values, compare ops, or depth ranges.
    struct DepthSettings {
        // ---- query helpers (hot path ¨C inline) ----

        // Depth clear value: standard = 1.0, reverse-z = 0.0
        static float ClearDepth(bool reverseZ) { return reverseZ ? 0.f : 1.f; }

        // Depth compare op for opaque geometry:
        //   standard  -> LESS_OR_EQUAL
        //   reverse-z -> GREATER_OR_EQUAL
        static rhi::CompareOp DepthCompareOp(bool reverseZ)
        {
            return reverseZ ? rhi::CompareOp::GREATER_OR_EQUAL : rhi::CompareOp::LESS_OR_EQUAL;
        }

        // ClearValue suitable for depth-stencil attachments
        static rhi::ClearValue DepthStencilClear(bool reverseZ)
        {
            return rhi::ClearValue(ClearDepth(reverseZ), 0);
        }

        // Min/max depth bounds (viewport depth range).
        // Both modes use [0, 1]; the projection matrix itself flips the mapping.
        static float MinDepth() { return 0.f; }
        static float MaxDepth() { return 1.f; }
    };

} // namespace sky

//
// Aurora RHI barrier helpers.
//

#include <aurora/rhi/Barrier.h>

namespace sky::aurora {

    namespace {
        // Per-class access masks for layout inference.
        constexpr AccessFlagBit COLOR_WRITE_BITS[] = {
            AccessFlagBit::COLOR_WRITE,
            AccessFlagBit::COLOR_INOUT_WRITE,
        };
        constexpr AccessFlagBit DEPTH_WRITE_BITS[] = {
            AccessFlagBit::DEPTH_STENCIL_WRITE,
            AccessFlagBit::DEPTH_STENCIL_INOUT_WRITE,
        };
        constexpr AccessFlagBit DEPTH_READ_BITS[] = {
            AccessFlagBit::DEPTH_STENCIL_READ,
            AccessFlagBit::DEPTH_STENCIL_INOUT_READ,
        };
        constexpr AccessFlagBit SRV_BITS[] = {
            AccessFlagBit::VERTEX_SRV,
            AccessFlagBit::FRAGMENT_SRV,
            AccessFlagBit::COMPUTE_SRV,
            AccessFlagBit::TASK_SRV,
            AccessFlagBit::MESH_SRV,
        };
        constexpr AccessFlagBit UAV_BITS[] = {
            AccessFlagBit::VERTEX_UAV_READ,
            AccessFlagBit::VERTEX_UAV_WRITE,
            AccessFlagBit::FRAGMENT_UAV_READ,
            AccessFlagBit::FRAGMENT_UAV_WRITE,
            AccessFlagBit::COMPUTE_UAV_READ,
            AccessFlagBit::COMPUTE_UAV_WRITE,
            AccessFlagBit::TASK_UAV_READ,
            AccessFlagBit::TASK_UAV_WRITE,
            AccessFlagBit::MESH_UAV_READ,
            AccessFlagBit::MESH_UAV_WRITE,
        };

        bool HasAny(AccessFlags access, std::initializer_list<AccessFlagBit> bits)
        {
            for (auto bit : bits) {
                if (access & bit) return true;
            }
            return false;
        }

        template <size_t N>
        bool HasAny(AccessFlags access, const AccessFlagBit (&bits)[N])
        {
            for (auto bit : bits) {
                if (access & bit) return true;
            }
            return false;
        }
    }

    ImageLayout InferLayoutForAccess(AccessFlags access)
    {
        if (access == AccessFlagBit::NONE) {
            return ImageLayout::UNDEFINED;
        }

        const bool hasColorWrite   = HasAny(access, COLOR_WRITE_BITS);
        const bool hasDepthWrite   = HasAny(access, DEPTH_WRITE_BITS);
        const bool hasDepthRead    = HasAny(access, DEPTH_READ_BITS);
        const bool hasSrv          = HasAny(access, SRV_BITS);
        const bool hasUav          = HasAny(access, UAV_BITS);
        const bool hasTransferRead = bool(access & AccessFlagBit::TRANSFER_READ);
        const bool hasTransferWrite= bool(access & AccessFlagBit::TRANSFER_WRITE);
        const bool hasPresent      = bool(access & AccessFlagBit::PRESENT);

        // Count classes; if > 1 distinct layout class is touched, fall to GENERAL.
        const int classes = (hasColorWrite ? 1 : 0)
                          + ((hasDepthWrite || hasDepthRead) ? 1 : 0)
                          + (hasSrv ? 1 : 0)
                          + (hasUav ? 1 : 0)
                          + (hasTransferRead ? 1 : 0)
                          + (hasTransferWrite ? 1 : 0)
                          + (hasPresent ? 1 : 0);

        if (classes == 0) {
            return ImageLayout::GENERAL;        // unknown bits set; play safe
        }
        if (hasUav) {
            return ImageLayout::GENERAL;        // any UAV use → GENERAL
        }
        if (classes > 1) {
            return ImageLayout::GENERAL;
        }

        if (hasColorWrite)    return ImageLayout::COLOR_ATTACHMENT;
        if (hasDepthWrite)    return ImageLayout::DEPTH_STENCIL_ATTACHMENT;
        if (hasDepthRead)     return ImageLayout::DEPTH_STENCIL_READ_ONLY;
        if (hasSrv)           return ImageLayout::SHADER_READ_ONLY;
        if (hasTransferRead)  return ImageLayout::TRANSFER_SRC;
        if (hasTransferWrite) return ImageLayout::TRANSFER_DST;
        if (hasPresent)       return ImageLayout::PRESENT;

        return ImageLayout::GENERAL;
    }

    bool IsLayoutCompatibleWithAccess(ImageLayout layout, AccessFlags access)
    {
        const ImageLayout inferred = InferLayoutForAccess(access);
        if (inferred == ImageLayout::GENERAL || inferred == ImageLayout::UNDEFINED) {
            return true;
        }
        return layout == inferred || layout == ImageLayout::GENERAL;
    }

} // namespace sky::aurora

//
// Aurora RHI barrier helpers.
//

#include <aurora/rhi/Barrier.h>

namespace sky::aurora {

    ImageLayout InferLayoutForAccess(AccessFlags access)
    {
        if (access == AccessFlagBit::NONE) {
            return ImageLayout::UNDEFINED;
        }

        const bool hasRtv     = bool(access & AccessFlagBit::RTV);
        const bool hasDsv     = bool(access & AccessFlagBit::DSV);
        const bool hasDsvRead = bool(access & AccessFlagBit::DSV_READ);
        const bool hasSrv     = bool(access & AccessFlagBit::SRV);
        const bool hasUav     = bool(access & AccessFlagBit::UAV);
        const bool hasCopySrc = bool(access & AccessFlagBit::COPY_SRC);
        const bool hasCopyDst = bool(access & AccessFlagBit::COPY_DST);
        const bool hasPresent = bool(access & AccessFlagBit::PRESENT);

        // Count distinct layout classes; if more than one is touched, fall back to GENERAL.
        const int classes = (hasRtv ? 1 : 0)
                          + ((hasDsv || hasDsvRead) ? 1 : 0)
                          + (hasSrv ? 1 : 0)
                          + (hasUav ? 1 : 0)
                          + (hasCopySrc ? 1 : 0)
                          + (hasCopyDst ? 1 : 0)
                          + (hasPresent ? 1 : 0);

        if (classes == 0) {
            return ImageLayout::GENERAL;   // CBV / vertex-input / unknown bits
        }
        if (hasUav) {
            return ImageLayout::GENERAL;   // any UAV use -> GENERAL
        }
        if (classes > 1) {
            return ImageLayout::GENERAL;
        }

        if (hasRtv)     return ImageLayout::COLOR_ATTACHMENT;
        if (hasDsv)     return ImageLayout::DEPTH_STENCIL_ATTACHMENT;
        if (hasDsvRead) return ImageLayout::DEPTH_STENCIL_READ_ONLY;
        if (hasSrv)     return ImageLayout::SHADER_READ_ONLY;
        if (hasCopySrc) return ImageLayout::TRANSFER_SRC;
        if (hasCopyDst) return ImageLayout::TRANSFER_DST;
        if (hasPresent) return ImageLayout::PRESENT;

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

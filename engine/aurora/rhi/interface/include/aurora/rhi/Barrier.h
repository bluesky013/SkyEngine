//
// Aurora RHI barrier helpers.
//

#pragma once

#include <aurora/rhi/Core.h>

namespace sky::aurora {

    // Map AccessFlags to the canonical ImageLayout for that access set.
    // Returns GENERAL when access bits span multiple incompatible layouts.
    ImageLayout InferLayoutForAccess(AccessFlags access);

    // Debug helper: true if the layout is compatible with all access bits in `access`.
    bool IsLayoutCompatibleWithAccess(ImageLayout layout, AccessFlags access);

} // namespace sky::aurora

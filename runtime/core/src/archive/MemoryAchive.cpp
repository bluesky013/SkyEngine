//
// Created by blues on 2024/1/14.
//

#include <core/archive/MemoryArchive.h>

namespace sky {

    const char* MemoryArchive::Data() const
    {
#ifdef _MSC_VER
        return ss.rdbuf()->view().data();
#else
        // TODO
        return nullptr;
#endif
    }

    uint32_t MemoryArchive::Size() const
    {
#ifdef _MSC_VER
        return static_cast<uint32_t>(ss.rdbuf()->view().size());
#else
        // TODO
        return 0;
#endif
    }

} // namespace sky

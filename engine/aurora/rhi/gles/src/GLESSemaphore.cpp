//
// Created on 2026/04/01.
//

#include <GLESSemaphore.h>

namespace sky::aurora {

    bool GLESSemaphore::Init(const Descriptor &desc)
    {
        counter = desc.initialValue;
        return true;
    }

} // namespace sky::aurora

//
// Created by Zach Lee on 2021/11/7.
//

#include "vulkan/DevObject.h"

namespace sky::drv {

    class Device;

    class Fence : public DevObject {
    public:
        Fence(Device&);
        ~Fence();
    };

}
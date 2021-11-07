//
// Created by Zach Lee on 2021/11/7.
//

#pragma once

#include "vulkan/DevObject.h"

namespace sky::drv {

    class Device;

    class Queue : public DevObject {
    public:
        Queue(Device&);
        ~Queue();
    };

}

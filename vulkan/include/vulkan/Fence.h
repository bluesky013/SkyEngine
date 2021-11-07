//
// Created by Zach Lee on 2021/11/7.
//

#include "vulkan/DevObject.h"
#include "vulkan/vulkan.h"

namespace sky::drv {

    class Device;

    class Fence : public DevObject {
    public:
        Fence(Device&);
        ~Fence();

        struct Descriptor {

        };

        bool Init(const Descriptor&);
    };

}
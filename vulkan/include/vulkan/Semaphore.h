//
// Created by Zach Lee on 2021/11/7.
//

#include "vulkan/DevObject.h"
#include "vulkan/vulkan.h"

namespace sky::drv {

    class Device;

    class Semaphore : public DevObject {
    public:
        Semaphore(Device&);
        ~Semaphore();

        struct Descriptor {

        };

        bool Init(const Descriptor&);
    };

}
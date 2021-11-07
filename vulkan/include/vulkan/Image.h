//
// Created by Zach Lee on 2021/11/7.
//

#include "vulkan/DevObject.h"

namespace sky::drv {

    class Device;

    class Image : public DevObject {
    public:
        Image(Device&);
        ~Image();
    };

}
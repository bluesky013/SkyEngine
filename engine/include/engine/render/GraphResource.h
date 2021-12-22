//
// Created by Zach Lee on 2021/12/22.
//

#pragma once

#include <vulkan/Image.h>

namespace sky {

    class GraphAttachment {
    public:
        GraphAttachment() = default;
        ~GraphAttachment() = default;

    private:
        uint32_t bindingFlag = 0;
    };

}
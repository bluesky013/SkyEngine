//
// Created by Zach Lee on 2022/11/10.
//

#pragma once

namespace sky::rhi {

    class CommandBuffer {
    public:
        CommandBuffer() = default;
        virtual ~CommandBuffer() = default;

        struct Descriptor {
        };
    };

}
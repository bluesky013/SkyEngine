//
// Created by Zach Lee on 2021/12/1.
//


#pragma once

#include <string>

namespace sky {

    class RenderView {
    public:
        struct Descriptor {
            std::string viewTag;
            uint32_t viewNum;
        };

        RenderView(Descriptor des) : descriptor(std::move(des)) {}
        ~RenderView() = default;

        const Descriptor& GetDescriptor() const;

    private:
        Descriptor descriptor;
    };

}
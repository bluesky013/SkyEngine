//
// Created by Zach Lee on 2021/12/1.
//


#pragma once

#include <string>

namespace sky {

    class RenderView {
    public:
        RenderView() = default;
        ~RenderView() = default;

        void SetViewTag(const std::string&);

    private:
        std::string viewTag;
        uint32_t num;
    };

}
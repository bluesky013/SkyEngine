//
// Created by Zach Lee on 2022/5/7.
//


#pragma once

#include <memory>

namespace sky {

    class RenderResource  {
    public:
        RenderResource() = default;
        virtual ~RenderResource() = default;

        virtual bool IsValid() const
        {
            return true;
        }
    };

}
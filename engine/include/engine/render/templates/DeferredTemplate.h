//
// Created by Zach Lee on 2021/12/22.
//

#pragma once
#include <engine/render/GraphTemplate.h>

namespace sky {

    class DeferredTemplate : public GraphTemplate {
    public:
        DeferredTemplate() = default;
        ~DeferredTemplate() = default;

        class DeferredConfig : public Config {
        public:
            DeferredConfig() = default;
            ~DeferredConfig() = default;
        };
    };


}
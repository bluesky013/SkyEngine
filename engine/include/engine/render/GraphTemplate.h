//
// Created by Zach Lee on 2021/12/22.
//

#pragma once
#include <list>
#include <vector>
#include <string>

namespace sky {

    class GraphTemplate {
    public:
        GraphTemplate() = default;
        ~GraphTemplate() = default;

        class Config {
        public:
            Config() = default;
            virtual ~Config() = default;
        };
    };

}
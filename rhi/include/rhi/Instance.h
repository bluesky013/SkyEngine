//
// Created by Zach Lee on 2022/11/10.
//

#pragma once

namespace sky::rhi {

    class Instance {
    public:
        Instance() = default;
        virtual ~Instance() = default;

        struct Descriptor {
            std::string appName;
            std::string engineName;
            bool        enableDebugLayer;
        };
    };

}
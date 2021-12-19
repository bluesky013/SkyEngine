//
// Created by Zach Lee on 2021/12/19.
//

#pragma once

#include <framework/environment/Singleton.h>

namespace sky {

    class ResourceManager : public Singleton<ResourceManager> {
    public:


    private:
        friend class Singleton<ResourceManager>;
        ResourceManager() = default;
        ~ResourceManager() = default;
    };

}
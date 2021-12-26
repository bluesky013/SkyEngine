//
// Created by Zach Lee on 2021/12/24.
//

#pragma once

#include <framework/environment/Singleton.h>
#include <vulkan/RenderPass.h>
#include <string>
#include <unordered_map>

namespace sky {

    class RenderPassLibrary : public Singleton<RenderPassLibrary> {
    public:
        void RegisterPass(const std::string& tag, drv::RenderPass*);

    private:
        friend class Singleton<RenderPassLibrary>;
        RenderPassLibrary() = default;
        ~RenderPassLibrary() = default;

        std::unordered_map<std::string, drv::RenderPass*> passes;
    };

}
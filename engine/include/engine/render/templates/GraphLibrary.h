//
// Created by Zach Lee on 2021/12/24.
//

#pragma once

#include <framework/environment/Singleton.h>
#include <vulkan/RenderPass.h>
#include <string>
#include <unordered_map>

namespace sky {

    class GraphLibrary : public Singleton<GraphLibrary> {
    public:
        void RegisterPass(const std::string& tag, drv::RenderPass*);

    private:
        friend class Singleton<GraphLibrary>;
        GraphLibrary() = default;
        ~GraphLibrary() = default;

        std::unordered_map<std::string, drv::RenderPass*> passes;
    };

}
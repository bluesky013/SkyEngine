//
// Created by Zach Lee on 2021/12/30.
//

#pragma once
#include <string>
#include <memory>

namespace sky {

    class RenderGraphNode {
    public:
        RenderGraphNode(std::string&& str) : name(std::move(str)) {}
        virtual ~RenderGraphNode() = default;

        const std::string& GetName() const
        {
            return name;
        }

        void AddRef()
        {
            ++counter;
        }

        void RemoveRef()
        {
            if (counter > 0) {
                --counter;
            }
        }

        void SideEffect() { sideEffect = true; }

        bool IsSideEffect() const { return sideEffect; }

        bool IsActive() const { return counter != 0 || sideEffect;  }

    private:
        std::string name;
        uint32_t counter;
        bool sideEffect = false;
    };
}
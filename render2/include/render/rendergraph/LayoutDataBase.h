//
// Created by Zach Lee on 2023/1/12.
//

#pragma once

#include <core/std/Container.h>
#include <rhi/DescriptorSetLayout.h>
#include <variant>

namespace sky {

    static constexpr uint32_t INVALID_VERTEX = ~(0U);

    struct LayoutPassBuilder {
        PmrResource *memoryResource = nullptr;
        uint32_t vertex = INVALID_VERTEX;
    };

    class LayoutPhaseBuilder {
        PmrResource *memoryResource = nullptr;
        uint32_t vertex = INVALID_VERTEX;
    };

    class LayoutDataBase {
    public:
        LayoutDataBase() = default;
        ~LayoutDataBase() = default;

        LayoutDataBase(const LayoutDataBase &) = delete;
        LayoutDataBase &operator=(const LayoutDataBase &) = delete;

        struct Node {
            std::string name;
            rhi::DescriptorSetLayoutPtr layout;
        };

        LayoutPassBuilder AddPass(const std::string &key);
        LayoutPhaseBuilder AddPhase(const std::string &key, const std::string &pass);

        void Compile();

    private:
        friend class RenderPipeline;
        PmrResource *memoryResource = nullptr;

        PmrList<Node> nodes;
    };

} // namespace sky

//
// Created by blues on 2024/9/3.
//

#pragma once

#include <render/resource/ResourceGroup.h>
#include <render/rdg/RenderGraphTypes.h>
#include <core/name/Name.h>

namespace sky {
    class RenderScene;
    namespace rdg {
        struct RenderGraph;
    }

    class PassBase {
    public:
        explicit PassBase(const Name &name_) : name(name_) {} // NOLINT
        virtual ~PassBase() = default;

        virtual void Prepare(rdg::RenderGraph &rdg, RenderScene &scene);
        virtual void Setup(rdg::RenderGraph &rdg, RenderScene &scene) {}
    protected:

        Name name;
        RDResourceLayoutPtr layout;

        std::vector<std::pair<Name, rdg::GraphImage>>  images;
        std::vector<std::pair<Name, rdg::GraphBuffer>> buffers;
    };

} // namespace sky
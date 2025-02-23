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

    class IPass {
    public:
        IPass(const Name &name_) : name(name_) {} // NOLINT
        virtual ~IPass() = default;

        virtual void Prepare(rdg::RenderGraph &rdg, RenderScene &scene) {};
        virtual void Setup(rdg::RenderGraph &rdg, RenderScene &scene) {}
    protected:

        Name name;
    };

    class PassBase : public IPass {
    public:
        explicit PassBase(const Name &name_) : IPass(name_) {} // NOLINT
        ~PassBase() = default;

        void Prepare(rdg::RenderGraph &rdg, RenderScene &scene) override;

    protected:
        RDResourceLayoutPtr layout;

        std::vector<std::pair<Name, rdg::GraphImage>>  images;
        std::vector<std::pair<Name, rdg::GraphBuffer>> buffers;
    };

} // namespace sky
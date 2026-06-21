//
// Aurora RHI PipelineLayout.
//

#pragma once

#include <core/template/ReferenceObject.h>
#include <aurora/rhi/Core.h>
#include <aurora/rhi/ResourceGroup.h>
#include <vector>

namespace sky::aurora {

    static constexpr uint32_t MAX_RESOURCE_GROUPS = 4;

    class PipelineLayout : public RefObject {
    public:
        struct Descriptor {
            std::vector<ResourceGroupLayout*>   groups;          // by set index; nullptr slots allowed
            std::vector<PushConstantRange>      pushConstants;
        };

        PipelineLayout() = default;
        ~PipelineLayout() override = default;
    };

    using PipelineLayoutPtr = CounterPtr<PipelineLayout>;

} // namespace sky::aurora

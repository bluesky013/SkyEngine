#pragma once

#include <aurora/renderer/RenderCommands.h>
#include <core/ecs/Dependency.h>

namespace sky::aurora {

    struct PrepareOrigin {
        SystemId system  = 0;
        size_t   chunk   = 0;
        size_t   ordinal = 0;
    };

    using DeferredEntityAction = std::variant<CreateRenderEntity, UpdateRenderEntity, DestroyRenderEntity>;
    struct DeferredEntityCommand {
        PrepareOrigin           origin;
        DeferredEntityAction    action;
        std::optional<uint64_t> expectedRevision;
    };
    struct DeferredSceneBatch {
        uint64_t                           sourceSequence = 0;
        std::vector<DeferredEntityCommand> commands;
    };

} // namespace sky::aurora

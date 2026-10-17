//
// Created on 2026/10/16.
//

#pragma once

#include "prepare/ScenePrepareScheduler.h"
#include <aurora/renderer/PreparedFrameTypes.h>

namespace sky::aurora {
    struct PreparedSceneProgram {
        CpuPrepareProgram                         work;
        PreparedFrame                             frame;
        std::vector<SystemId>                     viewSystems;
        std::vector<std::pair<SystemId, SceneId>> sceneSystems;
    };

    std::variant<RendererError, PreparedSceneProgram>                 BuildPreparedSceneProgram(const FrozenCpuFrame &,
                                                                                                std::shared_ptr<const std::vector<ResolvedResourceBinding>>,
                                                                                                size_t                   rowsPerChunk = 256,
                                                                                                std::span<const SceneId> paused       = {});
    std::variant<RendererError, std::shared_ptr<const PreparedFrame>> CollectPreparedScene(const PreparedSceneProgram &, const PrepareResult &);
} // namespace sky::aurora

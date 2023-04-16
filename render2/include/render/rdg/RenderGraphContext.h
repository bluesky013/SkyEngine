//
// Created by Zach Lee on 2023/4/16.
//

#pragma once

#include <memory>
#include <core/std/Container.h>
#include <render/rdg/TransientPool.h>

namespace sky::rdg {

    struct RenderGraphContext {
        PmrUnSyncPoolRes resources;
        std::unique_ptr<TransientPool> pool;
    };

} // namespace sky::rdg
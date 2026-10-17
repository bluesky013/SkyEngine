#pragma once

#include "scene/FrozenCpuFrame.h"

namespace sky::aurora {

    SystemDesc DescribeRenderObjectPrepare(SystemId id, SceneId scene);
    void       PrepareRenderObjects(const FrozenPrepareContext &context, SceneId scene);

} // namespace sky::aurora

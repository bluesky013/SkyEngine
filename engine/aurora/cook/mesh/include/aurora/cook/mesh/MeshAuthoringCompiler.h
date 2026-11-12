#pragma once

#include <memory>

namespace sky::phy {
    class IMeshAuthoringCompiler;
}
namespace sky::aurora::cook {
    std::shared_ptr<phy::IMeshAuthoringCompiler> CreateMeshAuthoringCompiler();
}

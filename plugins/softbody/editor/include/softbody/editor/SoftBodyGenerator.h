#pragma once

#include <memory>

namespace sky::editor {
    class IAssetGenerator;
}
namespace sky::aurora::editor {
    class IMeshGeneratorRecipeCodec;
}
namespace sky::softbody {
    std::shared_ptr<editor::IAssetGenerator>                   CreateSoftBodyGenerator();
    std::shared_ptr<aurora::editor::IMeshGeneratorRecipeCodec> CreateSoftBodyRecipeCodec();
} // namespace sky::softbody

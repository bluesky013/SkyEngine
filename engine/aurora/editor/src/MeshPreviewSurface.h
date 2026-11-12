#pragma once

#include <ui/IUITextureRegistry.h>

namespace sky::editor {
    struct EditorCamera;
}

namespace sky::phy {
    struct AuthoringMeshBinding;
    struct AuthoringMeshSource;
} // namespace sky::phy

namespace sky::aurora::editor {
    ui::UIImageData RasterizeMeshWeights(const phy::AuthoringMeshSource  &mesh,
                                         const phy::AuthoringMeshBinding &binding,
                                         const sky::editor::EditorCamera &camera,
                                         uint32_t                         width,
                                         uint32_t                         height,
                                         uint32_t                         bone);
}

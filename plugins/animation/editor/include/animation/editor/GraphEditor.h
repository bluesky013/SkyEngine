#pragma once

#include "animation/editor/GraphDocument.h"
#include <editor/ui/AssetViewProvider.h>

namespace sky::animation::editor {
    class CharacterPreview;
    std::unique_ptr<sky::editor::IAssetViewDocument> CreateGraphEditor(std::unique_ptr<GraphDocument>    document,
                                                                       std::shared_ptr<CharacterPreview> preview = {});
} // namespace sky::animation::editor

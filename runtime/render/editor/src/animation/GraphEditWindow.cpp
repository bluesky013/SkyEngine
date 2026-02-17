//
// Created by Zach Lee on 2026/1/11.
//

#include <render/adaptor/assets/AnimationAsset.h>
#include <render/editor/animation/GraphEditWindow.h>

#include "graph/AnimationGraphWidget.h"

namespace sky::editor {

    bool GraphEditWindow::SetupWidget(AssetPreviewWidget& widget, const AssetSourcePtr& src)
    {
        auto file = AssetDataBase::Get()->OpenFile(src);
        if (file) {
            widget.SetWidget(new AnimationGraphWidget(file));
            return true;
        }
        return false;
    }

} // namespace sky::editor


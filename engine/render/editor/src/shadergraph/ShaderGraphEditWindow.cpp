//
// Created by blues on 2026/3/10.
//

#include <render/editor/shadergraph/ShaderGraphEditWindow.h>
#include "ShaderGraphWidget.h"

#include <framework/asset/AssetDataBase.h>

namespace sky::editor {

    bool ShaderGraphEditWindow::SetupWidget(AssetPreviewWidget& widget, const AssetSourcePtr& src)
    {
        auto file = AssetDataBase::Get()->OpenFile(src);
        if (file) {
            widget.SetWidget(new ShaderGraphWidget(file));
            return true;
        }
        return false;
    }

} // namespace sky::editor

//
// Created by blues on 2026/3/10.
//

#pragma once

#include <editor/framework/AssetBrowserWidget.h>

namespace sky::editor {

    class ShaderGraphEditWindow : public IAssetPreviewWndFactory {
    public:
        ShaderGraphEditWindow() = default;
        ~ShaderGraphEditWindow() override = default;

        bool SetupWidget(AssetPreviewWidget& widget, const AssetSourcePtr& src) override;
    };

} // namespace sky::editor

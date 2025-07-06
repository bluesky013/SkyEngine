//
// Created by Zach Lee on 2025/6/30.
//

#pragma once

#include <editor/framework/AssetBrowserWidget.h>

namespace sky::editor {

    class SkeletonPreviewWindow : public IAssetPreviewWndFactory {
    public:
        SkeletonPreviewWindow();
        ~SkeletonPreviewWindow() override = default;

        bool SetupWidget(AssetPreviewWidget& widget, const AssetSourcePtr& src) override;
    };

} // namespace sky::editor

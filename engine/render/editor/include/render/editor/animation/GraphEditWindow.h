//
// Created by Zach Lee on 2026/1/11.
//

#pragma once

#include <editor/framework/AssetBrowserWidget.h>

namespace sky::editor {

    class GraphEditWindow : public IAssetPreviewWndFactory {
    public:
        GraphEditWindow() = default;
        ~GraphEditWindow() override = default;

        bool SetupWidget(AssetPreviewWidget& widget, const AssetSourcePtr& src) override;
    };

} // namespace sky::editor
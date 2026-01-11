//
// Created by Zach Lee on 2026/1/11.
//

#pragma once

#include <editor/framework/AssetBrowserWidget.h>
#include <render/adaptor/assets/AnimationAsset.h>

#include <QtNodes/DataFlowGraphicsScene>
#include <QtNodes/ConnectionStyle>
#include <QtNodes/GraphicsView>
#include <QtNodes/NodeDelegateModelRegistry>

namespace sky::editor {

    class AnimationGraphModel;

    class AnimationGraphWidget : public AssetPreviewContentWidget {
        Q_OBJECT
    public:
        explicit AnimationGraphWidget(const FilePtr& source);

    private:
        void OnClose() override {}

        std::shared_ptr<QtNodes::NodeDelegateModelRegistry> registry;

        QtNodes::DataFlowGraphModel* model;
        QtNodes::DataFlowGraphicsScene* scene;
        QtNodes::GraphicsView* view;

        FilePtr asset;
        AnimationAssetData data;
    };

    class StateMachineWidget : public QWidget {
        Q_OBJECT
    public:
        explicit StateMachineWidget();

    private:
    };

} // sky::editor


//
// Created by blues on 2026/3/10.
//

#pragma once

#include <editor/framework/AssetBrowserWidget.h>
#include <shader/shadergraph/ShaderGraph.h>

#include <QtNodes/DataFlowGraphicsScene>
#include <QtNodes/GraphicsView>
#include <QtNodes/NodeDelegateModelRegistry>
#include <QtNodes/DataFlowGraphModel>

namespace sky::editor {

    class ShaderGraphWidget : public AssetPreviewContentWidget {
        Q_OBJECT
    public:
        explicit ShaderGraphWidget(const FilePtr& source);

    private:
        void OnClose() override {}

        std::shared_ptr<QtNodes::NodeDelegateModelRegistry> registry;

        QtNodes::DataFlowGraphModel*    model;
        QtNodes::DataFlowGraphicsScene* scene;
        QtNodes::GraphicsView*          view;

        FilePtr          asset;
        sg::ShaderGraph  graph;
    };

} // namespace sky::editor

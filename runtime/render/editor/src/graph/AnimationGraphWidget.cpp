//
// Created by Zach Lee on 2026/1/11.
//

#include "AnimationGraphWidget.h"

#include <QVBoxLayout>

namespace sky::editor {

    static std::shared_ptr<QtNodes::NodeDelegateModelRegistry> RegisterDataModels()
    {
        auto ret = std::make_shared<QtNodes::NodeDelegateModelRegistry>();
        return ret;
    }

    AnimationGraphWidget::AnimationGraphWidget(const FilePtr& source)
        : AssetPreviewContentWidget()
        , model(new QtNodes::DataFlowGraphModel(RegisterDataModels()))
        , scene(new QtNodes::DataFlowGraphicsScene(*model))
        , view(new QtNodes::GraphicsView(scene))
        , asset(source)
        , data{}
    {
        auto *mainLayout = new QVBoxLayout(this);
        mainLayout->addWidget(view);
        setLayout(mainLayout);
        setBaseSize(800, 600);

        connect(scene, &QtNodes::DataFlowGraphicsScene::modified, this, [this]() {
            setWindowModified(true);
        });
    }

} // namespace sky::editor
//
// Created by Zach Lee on 2021/12/15.
//

#pragma once

#include <QDockWidget>
#include <vector>
#include <editor/dockwidget/WorldWidget.h>
#include <editor/inspector/InspectorBase.h>

class QVBoxLayout;
class QPushButton;
class QScrollArea;

namespace sky {
    class World;
    class Actor;
}

namespace sky::editor {

    class InspectorWidget : public QDockWidget {
        Q_OBJECT
    public:
        explicit InspectorWidget(QWidget* parent);
        ~InspectorWidget() override = default;

        void AddComponent(ComponentBase* comp);

        void Clear();

    public Q_SLOTS:
        void OnSelectedItemChanged(ActorPtr actor);

    private:
        void Refresh();
        void OnAddComponentClicked();

        QWidget* groupWidget;
        QPushButton *button;
        std::vector<InspectorBase*> groups;

        ActorPtr actor = nullptr;
    };

}
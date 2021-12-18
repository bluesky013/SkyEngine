//
// Created by Zach Lee on 2021/12/16.
//

#pragma once
#include <QWidget>
#include <QGridLayout>

namespace sky {
    struct TypeMemberNode;
}

class QLabel;

namespace sky::editor {

    class PropertyWidget : public QWidget {
    public:
        PropertyWidget(QWidget* parent);
        ~PropertyWidget() = default;

        virtual void SetInstance(void* instance, const QString&, const TypeMemberNode& node);

        virtual void Refresh() {}

    protected:
        void* instance;
        const TypeMemberNode* memberNode;
        QLabel* label;
    };

}

//
// Created by Zach Lee on 2021/12/16.
//

#pragma once
#include <QWidget>
#include <QGridLayout>

namespace sky {
    struct TypeNode;
}

class QLabel;
class QHBoxLayout;

namespace sky::editor {

    class PropertyWidget : public QWidget {
    public:
        explicit PropertyWidget(void *ptr, const TypeNode *node, QWidget* parent);
        explicit PropertyWidget(QWidget* parent);
        ~PropertyWidget() override = default;

        void SetInstance(void* instance, const TypeNode *node);
        void SetLabel(const QString &str);

        virtual void Refresh();

    protected:
        QHBoxLayout *layout;
        QLabel *label = nullptr;

        void* instance;
        const TypeNode* typeNode;
        QList<PropertyWidget *> children;
    };

}

//
// Created by Zach Lee on 2021/12/15.
//

#pragma once

#include <QWidget>
#include <framework/serialization/SerializationContext.h>

class QLabel;
class QVBoxLayout;

namespace sky::editor {

    class InspectorBase : public QWidget {
    public:
        explicit InspectorBase(QWidget* parent);
        ~InspectorBase() override = default;

        void SetObject(void *ptr, const TypeNode *node);

    private:
        QLabel* label;
        QVBoxLayout* layout;
    };

}
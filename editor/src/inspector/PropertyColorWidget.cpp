//
// Created by Zach Lee on 2023/8/23.
//

#include <editor/inspector/PropertyColorWidget.h>
#include <QColorDialog>
#include <core/platform/Platform.h>
#include <framework/serialization/SerializationContext.h>

namespace sky::editor {

    PropertyColorWidget::PropertyColorWidget(void *inst, const TypeNode *node, QWidget* parent) : PropertyWidget(inst, node, parent)
    {
        layout->setSpacing(0);
        layout->addWidget(label, 0, Qt::AlignLeft | Qt::AlignTop);

        button = new QPushButton(this);
        layout->addWidget(button);
        connect(button, &QPushButton::clicked, this, [this](bool checked) {
            QColorDialog dialog;
            if (dialog.exec() != 0) {
                auto color = dialog.selectedColor();
                auto *val = reinterpret_cast<float *>(instance);
                val[0] = static_cast<float>(color.redF());
                val[1] = static_cast<float>(color.greenF());
                val[2] = static_cast<float>(color.blueF());
                val[3] = static_cast<float>(color.alphaF());
                Refresh();
            }
        });
    }

    void PropertyColorWidget::Refresh()
    {
        const auto *val = reinterpret_cast<const float *>(instance);
        QColor color;
        color.setRedF(val[0]);
        color.setGreenF(val[1]);
        color.setBlueF(val[2]);
        color.setAlphaF(val[3]);
        QString qss = QString("background-color: %1").arg(color.name());
        button->setStyleSheet(qss);
    }
} // sky::editor
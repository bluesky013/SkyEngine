//
// Created by Zach Lee on 2021/12/15.
//

#include <QApplication>
#include <QLabel>
#include <QPainter>
#include <QStyle>
#include <QVBoxLayout>
#include <editor/inspector/InspectorBase.h>

namespace sky::editor {

    InspectorBase::InspectorBase(QWidget *parent) : QWidget(parent)
    {
        auto layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);

        label = new QLabel(this);
        label->setStyleSheet("background-color: #333333; border-radius: 8px; border-style: solid; border-color: #1B1B1B; border-width: 1px; "
                             "border-left: none; border-right: none;");
        label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        label->setFixedHeight(24);
        label->setMargin(0);

        layout->addWidget(label);
    }

    void InspectorBase::SetName(const QString &str)
    {
        label->setText(str);
    }
} // namespace sky::editor
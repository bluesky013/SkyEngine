//
// Created by Zach Lee on 2021/12/12.
//

#include "CentralWidget.h"
#include <QVBoxLayout>

namespace sky::editor {

    CentralWidget::CentralWidget(QWidget *parent) : QWidget(parent)
    {
        layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
    }

    CentralWidget::~CentralWidget() = default;

    void CentralWidget::Init()
    {
    }

} // namespace sky::editor
//
// Created by blues on 2024/11/29.
//

#include "ToolBar.h"
#include <QLabel>
#include <editor/framework/EditorToolBase.h>
#include <framework/window/NativeWindow.h>

namespace sky::editor {

    ToolBar::ToolBar(QWidget *parent, QDockWidget *dockWidget) : QToolBar(parent), toolDockWidget(dockWidget)
    {
        setMinimumHeight(64);
        setMinimumWidth(64);

        comboBox = new QComboBox(this);
        comboBox->setMinimumWidth(128);
        addWidget(comboBox);
        addSeparator();

        const auto &tools = EditorToolManager::Get()->GetTools();
        for (const auto &tool : tools) {
            auto name = QString(tool.first.GetStr().data());
            comboBox->addItem(name);

            auto *toolWidget = tool.second->InitToolWidget(toolDockWidget);
            toolMap.insert(name, toolWidget);
        }
        connect(comboBox, &QComboBox::currentTextChanged, this, [this](const QString& text) {
            ResetToolWidget(text);
        });
        comboBox->setCurrentText("Select");

        binder.Bind(this);
    }

    void ToolBar::SetWorld(WorldTreeView* world)
    {
        for (auto &tool : toolMap) {
            if (tool != nullptr) {
                tool->SetWorld(world);
            }
        }
    }

    void ToolBar::SetCamera(EditorCamera *camera)
    {
        for (auto &tool : toolMap) {
            if (tool != nullptr) {
                tool->SetCamera(camera);
            }
        }
    }

    void ToolBar::ResetToolWidget(const QString &name)
    {
        toolDockWidget->setWindowTitle(name);

        auto *last = static_cast<ToolWidget*>(toolDockWidget->widget());
        if (last != nullptr) {
            last->DeActivate();
        }

        auto *widget = toolMap[name];
        if (widget != nullptr) {
            widget->Activate(windowId);
        }
        toolDockWidget->setVisible(widget != nullptr);
        toolDockWidget->setWidget(widget);
    }

    void ToolBar::OnMainWindowCreated(NativeWindow *window)
    {
        windowId = window->GetWinId();
        ResetToolWidget(comboBox->currentText());
    }

} // namespace sky::editor

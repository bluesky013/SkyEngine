//
// Created by Zach Lee on 2021/12/15.
//

#include <QMenu>
#include <QPushButton>
#include <QVBoxLayout>
#include <QScrollArea>
#include <utility>
#include <editor/inspector/ComponentInspector.h>
#include <editor/dockwidget/InspectorWidget.h>

#include <framework/world/Component.h>
#include <framework/world/ComponentFactory.h>

namespace sky::editor {

    InspectorWidget::InspectorWidget(QWidget *parent) : QDockWidget(parent)
    {
        setWindowTitle(tr("Inspector"));

        auto *widget = new QWidget(this);
        widget->setStyleSheet("background-color:white;");
        widget->setAcceptDrops(true);
        widget->setLayout(new QVBoxLayout(widget));
        widget->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Minimum);
        setWidget(widget);

        auto *rootLayout = widget->layout();
        rootLayout->setAlignment(Qt::AlignTop);

        button = new QPushButton(tr("Add Component"), widget);
        connect(button, &QPushButton::clicked, this, &InspectorWidget::OnAddComponentClicked);

        groupWidget = new QWidget(widget);
        auto *groupLayout = new QVBoxLayout(groupWidget);
        groupLayout->setAlignment(Qt::AlignTop);
        groupWidget->setLayout(groupLayout);

        rootLayout->addWidget(groupWidget);
        rootLayout->addWidget(button);

        // auto refreshTimer = new QTimer(this);
        // connect(refreshTimer, &QTimer::timeout, this, [this]() {
        //     Refresh();
        // });
        // refreshTimer->setInterval(1000);
        // refreshTimer->start();
    }

    void InspectorWidget::AddComponent(ComponentBase *comp)
    {
        auto *inspector = new ComponentInspector(groupWidget);
        inspector->SetComponent(comp);
        groupWidget->layout()->addWidget(inspector);
        groups.emplace_back(inspector);
    }

    void InspectorWidget::Clear()
    {
        while (QLayoutItem *child = groupWidget->layout()->takeAt(0)) {
            delete child->widget();
            delete child;
        }
        groups.clear();
    }

    void InspectorWidget::OnSelectedItemChanged(ActorPtr actor_)
    {
        actor = std::move(actor_);
        Refresh();
    }

    void InspectorWidget::OnAddComponentClicked()
    {
        QMenu menu(tr("Components"), this);

        const auto &typeMap = ComponentFactory::Get()->GetTypes();
        for (const auto &[group, types] : typeMap) {
            auto *sub = menu.addMenu(group.c_str());

            for (const auto &type : types) {
                auto *action = new QAction(type.name.data(), sub);
                connect(action, &QAction::triggered, this, [this, typeId = type.typeId](bool checked) {
                    if (actor != nullptr) {
                        actor->AddComponent(typeId);
                        Refresh();
                    }
                });

                sub->addAction(action);
            }
        }

        menu.exec(mapToGlobal(button->pos()));
    }

    void InspectorWidget::Refresh()
    {
        Clear();

        if (actor) {
            const auto &comps = actor->GetComponents();
            for (const auto &[id, comp] : comps) {
                AddComponent(comp.get());
            }
        }
    }

} // namespace sky::editor

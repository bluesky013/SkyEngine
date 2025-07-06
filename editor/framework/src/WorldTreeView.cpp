
//
// Created by blues on 2024/7/6.
//

#include <editor/framework/WorldTreeView.h>
#include <framework/world/TransformComponent.h>
#include <framework/interface/ISelectEvent.h>
#include <QBoxLayout>
#include <QMenu>

namespace sky::editor {

    void TreeViewComponent::Reflect(sky::SerializationContext *context)
    {
        REGISTER_BEGIN(TreeViewComponent, context);
    }

    WorldTreeView::WorldTreeView(QWidget *parent) : QWidget(parent)
    {
        treeView = new QTreeView(this);
        treeView->setHeaderHidden(true);
        treeView->setSelectionMode(QAbstractItemView::SelectionMode::ExtendedSelection);
        treeView->setDragDropMode(QAbstractItemView::DragDropMode::DragDrop);
        treeView->setDefaultDropAction(Qt::DropAction::MoveAction);

        auto *layout = new QVBoxLayout(this);
        setLayout(layout);
        layout->addWidget(treeView);

        setContextMenuPolicy(Qt::CustomContextMenu);

        model = new QStandardItemModel(this);
        treeView->setModel(model);
        connect(model, &QStandardItemModel::itemChanged, this, &WorldTreeView::OnItemChanged);
        connect(treeView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &WorldTreeView::OnSelectItemChanged);
        connect(this, &QWidget::customContextMenuRequested, this, &WorldTreeView::OnContentMenuClicked);
    }

    void WorldTreeView::OnSelectItemChanged(const QItemSelection &selected, const QItemSelection &deselected)
    {
        auto indexes = selected.indexes();
        
        WorldActorItem* item = nullptr;
        if (indexes.size() == 1) {
            auto *sItem = model->itemFromIndex(indexes[0]);
            item = dynamic_cast<WorldActorItem*>(sItem);
        }
        
        if (item != nullptr) {
            emit WorldTreeSelectItemChanged(item != nullptr ? item->actor : nullptr);
            SelectEvent::BroadCast(&ISelectEvent::OnActorSelected, item->actor.get());
        } else {
            emit WorldTreeSelectItemChanged(nullptr);
            SelectEvent::BroadCast(&ISelectEvent::OnActorSelected, nullptr);
        }
    }

    void WorldTreeView::BuildByWorld(const WorldPtr &world)
    {
        attachedWorld = world;

        if (world) {
            worldEvent.Bind(this, attachedWorld.Get());
        } else {
            worldEvent.Reset();
        }

        RebuildTree();
    }

    void WorldTreeView::RebuildTree()
    {
        model->clear();

        std::unordered_map<Uuid, WorldActorItem*> itemMap;
        if (attachedWorld) {
            const auto &actors = attachedWorld->GetActors();
            for (const auto &actor: actors) {
                auto *item = new WorldActorItem(actor);
                itemMap.emplace(actor->GetUuid(), item);
            }
        }

        for (auto &[id, item] : itemMap) {
            auto *trans = item->actor->GetComponent<TransformComponent>();
            WorldActorItem *parent = nullptr;
            if (auto *parentTrans = trans != nullptr ? trans->GetParent() : nullptr; parentTrans != nullptr) {
                parent = itemMap.at(parentTrans->GetActor()->GetUuid());
            }
            if (parent != nullptr) {
                parent->appendRow(item);
            } else {
                model->appendRow(item);
            }
        }
    }

    void WorldTreeView::GatherAllChildren(std::vector<ActorPtr> &actors, QStandardItem *item) // NOLINT
    {
        actors.emplace_back(static_cast<WorldActorItem*>(item)->actor);
        int count = item->rowCount();
        for (int i = 0; i < count; ++i) {
            GatherAllChildren(actors, item->child(i));
        }
    }

    ActorPtr WorldTreeView::AddActor(const QString &name)
    {
        auto indices = treeView->selectionModel()->selectedIndexes();
        auto actor = attachedWorld->CreateActor(name.toStdString());

        auto *parent = indices.empty() ? model->invisibleRootItem() : model->itemFromIndex(indices[0]);
        auto parentAct = indices.empty() ? ActorPtr{} : static_cast<WorldActorItem*>(parent)->actor;
        actor->SetParent(parentAct);

        return actor;
    }

    void WorldTreeView::OnActorAttached(const ActorPtr &actor)
    {
        auto *item = new WorldActorItem(actor);
        model->invisibleRootItem()->appendRow(item);
        actor->AddComponent<TreeViewComponent>(model->indexFromItem(item));
    }

    void WorldTreeView::OnActorDetached(const ActorPtr &actor)
    {
        auto &idx = actor->GetComponent<TreeViewComponent>()->index;
        model->removeRow(idx.row(), idx.parent());
    }

    void WorldTreeView::OnContentMenuClicked(const QPoint &pos)
    {
        if (attachedWorld == nullptr) {
            return;
        }

        QMenu menu(tr("World Action"), this);

        auto *addAct = new QAction(tr("Add"), &menu);
        connect(addAct, &QAction::triggered, this, &WorldTreeView::AddActorEmpty);


        auto *delAct = new QAction(tr("Delete"), &menu);
        connect(delAct, &QAction::triggered, this, [this]() {
            auto indices = treeView->selectionModel()->selectedIndexes();

            for (auto &index : indices) {
                std::vector<ActorPtr> actorsToDel;
                GatherAllChildren(actorsToDel, model->itemFromIndex(index));
                for (auto &act : actorsToDel)  {
                    attachedWorld->DetachFromWorld(act);
                }
            }
        });

        menu.addSeparator();
        auto *collapseAct = new QAction(tr("Collapse"), &menu);
        connect(collapseAct, &QAction::triggered, this, [this]() { treeView->collapseAll(); });
        auto *expandAct = new QAction(tr("Expand"), &menu);
        connect(expandAct, &QAction::triggered, this, [this]() { treeView->expandAll(); });


        menu.addAction(addAct);
        menu.addAction(delAct);
        menu.addAction(collapseAct);
        menu.addAction(expandAct);

        menu.exec(mapToGlobal(pos));
    }

    void WorldTreeView::OnItemChanged(QStandardItem *item)
    {
        auto *aItem = dynamic_cast<WorldActorItem*>(item);
        if (aItem != nullptr) {
            aItem->actor->SetName(item->text().toStdString());

            auto *parent = dynamic_cast<WorldActorItem*>(item->parent());
            if (parent != nullptr) {
                aItem->actor->SetParent(parent->actor);
            }
        }
    }

} // namespace sky::editor

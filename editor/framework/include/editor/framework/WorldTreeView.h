//
// Created by blues on 2024/7/6.
//

#pragma once

#include <QTreeView>
#include <QStandardItemModel>
#include <QStandardItem>
#include <framework/world/World.h>

namespace sky::editor {

    class WorldActorItem : public QStandardItem {
    public:
        explicit WorldActorItem(const ActorPtr& actor_) : QStandardItem(QString().fromStdString(actor_->GetName())), actor(actor_) {}
        ~WorldActorItem() override = default;

        ActorPtr actor;
    };

    class WorldTreeView : public QWidget {
        Q_OBJECT
    public:
        explicit WorldTreeView(QWidget *parent);
        ~WorldTreeView() override = default;

        void BuildByWorld(const WorldPtr &world);
        void RebuildTree();

        ActorPtr AddActor(const QString &name);
        void AddActorEmpty() { AddActor("Actor"); }

        World *GetWorld() { return attachedWorld.Get(); }

    Q_SIGNALS:
        void WorldTreeSelectItemChanged(ActorPtr actor); // NOLINT

    private:
        void GatherAllChildren(std::vector<ActorPtr> &actors, QStandardItem *item);

        void OnItemChanged(QStandardItem *item);
        void OnContentMenuClicked(const QPoint &pos);
        void OnSelectItemChanged(const QItemSelection &selected, const QItemSelection &deselected);

        QStandardItemModel *model = nullptr;
        QTreeView *treeView = nullptr;

        WorldPtr attachedWorld;
    };

} // namespace sky::editor
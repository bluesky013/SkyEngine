//
// Created by blues on 2024/7/6.
//

#pragma once

#include <QTreeView>
#include <QStandardItemModel>
#include <QStandardItem>
#include <framework/world/World.h>
#include <framework/world/Actor.h>
#include <framework/world/Component.h>
#include <core/event/Event.h>

namespace sky::editor {

    struct TreeViewComponent : public ComponentBase {
    public:
        explicit TreeViewComponent(const QModelIndex& idx) : index(idx) {}
        ~TreeViewComponent() override = default;

        COMPONENT_RUNTIME_INFO(TreeViewComponent)

        static void Reflect(SerializationContext *context);

        QModelIndex index;
    };

    class WorldActorItem : public QStandardItem, public IActorEvent {
    public:
        explicit WorldActorItem(const ActorPtr& actor_) : QStandardItem(QString().fromStdString(actor_->GetName())), actor(actor_)
        {
            binder.Bind(this, actor.get());
        }

        ~WorldActorItem() override = default;

        void OnParentChanged(Actor* oldParent, Actor* newParent) override
        {
            if (oldParent == newParent) {
                return;
            }

            auto &currentIdx = actor->GetComponent<TreeViewComponent>()->index;

            auto *parent = model()->invisibleRootItem();
            if (oldParent != nullptr) {
                auto &oldIdx = oldParent->GetComponent<TreeViewComponent>()->index;
                model()->removeRow(currentIdx.row(), oldIdx);
            }

            if (newParent != nullptr) {
                auto &newIdx = newParent->GetComponent<TreeViewComponent>()->index;
                parent = model()->itemFromIndex(newIdx);
            }
            parent->appendRow(this);
        }

        ActorPtr actor;
        EventBinder<IActorEvent> binder;
    };

    class WorldTreeView : public QWidget, public IWorldEvent {
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

        void OnActorAttached(const ActorPtr &actor) override;
        void OnActorDetached(const ActorPtr &actor) override;

        QStandardItemModel *model = nullptr;
        QTreeView *treeView = nullptr;

        WorldPtr attachedWorld;
        EventBinder<IWorldEvent> worldEvent;
    };

} // namespace sky::editor
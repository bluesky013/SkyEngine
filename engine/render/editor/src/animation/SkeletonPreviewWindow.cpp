//
// Created by Zach Lee on 2025/6/30.
//

#include <render/editor/animation/SkeletonPreviewWindow.h>
#include <render/adaptor/assets/SkeletonAsset.h>
#include <editor/framework/ViewportWidget.h>
#include <editor/framework/ReflectedObjectWidget.h>
#include <QVBoxLayout>
#include <QStandardItemModel>
#include <QHBoxLayout>
#include <QTreeView>

namespace sky::editor {

    class SkeletonPreviewWidget : public AssetPreviewContentWidget {
    public:
        explicit SkeletonPreviewWidget(const FilePtr& source) : asset(source)
        {
            auto archive = source->ReadAsArchive();
            JsonInputArchive bin(*archive);
            data.LoadJson(bin);

            auto *layout = new QHBoxLayout(this);
            setLayout(layout);

            model = new QStandardItemModel();
            boneTree = new QTreeView(this);
            boneTree->setModel(model);
            boneTree->setHeaderHidden(true);
            boneTree->setEditTriggers(QAbstractItemView::NoEditTriggers);
            boneTree->setDragDropMode(QAbstractItemView::NoDragDrop);

            layout->addWidget(boneTree);
            BuildBoneHierarchy();
        }

        ~SkeletonPreviewWidget() override
        {
        }

        void OnClose() override
        {
        }

        void BuildBoneHierarchy()
        {
            std::vector<QStandardItem*> items(data.boneData.size(), nullptr);

            for (size_t i = 0; i < data.boneData.size(); ++i) {
                const auto& bone = data.boneData[i];
                items[i] = new QStandardItem(bone.name.GetStr().data());
                items[i]->setData(static_cast<BoneIndex>(i));
            }

            for (size_t i = 0; i < data.boneData.size(); ++i) {
                const auto& bone = data.boneData[i];
                const auto& boneItem = items[i];

                if (bone.parentIndex != INVALID_BONE_ID) {
                    items[bone.parentIndex]->appendRow(boneItem);
                } else {
                    model->appendRow(boneItem);
                }
            }

            Skeleton::BuildSkeleton(data);
        }

    private:
        QTreeView* boneTree = nullptr;
        QStandardItemModel* model = nullptr;

        FilePtr asset;
        SkeletonAssetData data;
    };

    bool SkeletonPreviewWindow::SetupWidget(AssetPreviewWidget& widget, const AssetSourcePtr& src)
    {
        auto file = AssetDataBase::Get()->OpenFile(src);
        if (file) {
            widget.SetWidget(new SkeletonPreviewWidget(file));
            return true;
        }
        return false;
    }

} // namespace sky::editor

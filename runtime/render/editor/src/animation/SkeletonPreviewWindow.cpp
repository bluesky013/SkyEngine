//
// Created by Zach Lee on 2025/6/30.
//

#include <render/editor/animation/SkeletonPreviewWindow.h>
#include <render/adaptor/assets/SkeletonAsset.h>
#include <framework/asset/AssetManager.h>
#include <framework/asset/Asset.h>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QStandardItemModel>

namespace sky::editor {

    class SkeletonPreviewWidget : public QWidget {
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

            layout->addWidget(boneTree);

            BuildBoneHierarchy();
        }
        ~SkeletonPreviewWidget() = default;

        void BuildBoneHierarchy()
        {
            std::vector<QStandardItem*> items(data.boneData.size(), nullptr);

            for (size_t i = 0; i < data.boneData.size(); ++i) {
                const auto& bone = data.boneData[i];
                items[i] = new QStandardItem(bone.name.GetStr().data());
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
        }

    private:
        QTreeView* boneTree = nullptr;
        QStandardItemModel* model = nullptr;

        FilePtr asset;
        SkeletonAssetData data;
    };

    SkeletonPreviewWindow::SkeletonPreviewWindow()
    {
    }

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

//
// Created by blues on 2024/7/7.
//

#include <editor/framework/ReflectedObjectWidget.h>

#include <QDropEvent>
#include <QMimeData>

#include <core/math/MathUtil.h>
#include <framework/serialization/SerializationUtil.h>
#include <framework/serialization/PropertyCommon.h>

namespace sky::editor {

    ReflectedObjectWidget::ReflectedObjectWidget(void *obj, const TypeNode *node, QWidget *parent)
        : QWidget(parent)
        , object(obj)
        , typeNode(node)
    {
        auto *layout = new QFormLayout(this);
        layout->setLabelAlignment(Qt::AlignLeft);
        layout->setFormAlignment(Qt::AlignCenter);
        setLayout(layout);

        for (const auto &[name, memberInfo] : typeNode->members) {
            ReflectedMemberWidget *widget = nullptr;
            if (memberInfo.info->staticInfo->isEnum) {
                widget = new PropertyEnum(object, &memberInfo, this);
            } else if (memberInfo.info->registeredId == TypeInfo<bool>::RegisteredId()) {
                widget = new PropertyBool(object, &memberInfo, this);
            } else if (memberInfo.info->registeredId == TypeInfo<uint64_t>::RegisteredId()) {
                widget = new PropertyScalar<uint64_t>(object, &memberInfo, this);
            } else if (memberInfo.info->registeredId == TypeInfo<uint32_t>::RegisteredId()) {
                widget = new PropertyScalar<uint32_t>(object, &memberInfo, this);
            } else if (memberInfo.info->registeredId == TypeInfo<uint16_t>::RegisteredId()) {
                widget = new PropertyScalar<uint16_t>(object, &memberInfo, this);
            } else if (memberInfo.info->registeredId == TypeInfo<uint8_t>::RegisteredId()) {
                widget = new PropertyScalar<uint8_t>(object, &memberInfo, this);
            } else if (memberInfo.info->registeredId == TypeInfo<int64_t>::RegisteredId()) {
                widget = new PropertyScalar<int64_t>(object, &memberInfo, this);
            } else if (memberInfo.info->registeredId == TypeInfo<int32_t>::RegisteredId()) {
                widget = new PropertyScalar<int32_t>(object, &memberInfo, this);
            } else if (memberInfo.info->registeredId == TypeInfo<int16_t>::RegisteredId()) {
                widget = new PropertyScalar<int16_t>(object, &memberInfo, this);
            } else if (memberInfo.info->registeredId == TypeInfo<int8_t>::RegisteredId()) {
                widget = new PropertyScalar<int8_t>(object, &memberInfo, this);
            } else if (memberInfo.info->registeredId == TypeInfo<float>::RegisteredId()) {
                widget = new PropertyScalar<float>(object, &memberInfo, this);
            } else if (memberInfo.info->registeredId == TypeInfo<double>::RegisteredId()) {
                widget = new PropertyScalar<double>(object, &memberInfo, this);
            } else if (memberInfo.info->registeredId == TypeInfo<Vector2>::RegisteredId()) {
                widget = new PropertyVec<2>(object, &memberInfo, this);
            } else if (memberInfo.info->registeredId == TypeInfo<Vector3>::RegisteredId()) {
                widget = new PropertyVec<3>(object, &memberInfo, this);
            } else if (memberInfo.info->registeredId == TypeInfo<Vector4>::RegisteredId()) {
                widget = new PropertyVec<4>(object, &memberInfo, this);
            } else if (memberInfo.info->registeredId == TypeInfo<Color>::RegisteredId()) {
                widget = new PropertyColor(object, &memberInfo, this);
            } else if (memberInfo.info->registeredId == TypeInfo<Uuid>::RegisteredId()) {
                widget = new PropertyUuid(object, &memberInfo, this);
            } else if (memberInfo.info->registeredId == TypeInfo<std::string>::RegisteredId()) {
            }

            if (widget != nullptr) {
                layout->addRow(name.data(), widget);
            }
        }
    }

    void ReflectedObjectWidget::Refresh()
    {
    }

    void AssetLineEditor::dragEnterEvent(QDragEnterEvent *event)
    {
        if (event->mimeData()->hasUrls()) {
            event->acceptProposedAction();
        }
    }

    void AssetLineEditor::dragMoveEvent(QDragMoveEvent *event)
    {
        if (event->mimeData()->hasUrls()) {
            event->acceptProposedAction();
        }
    }

    void AssetLineEditor::dropEvent(QDropEvent *event)
    {
        std::set<QString> filterList = {
            "project",
            "engine"
        };

        const auto *mimeData = event->mimeData();
        if (mimeData->hasUrls()) {
            auto sch = mimeData->urls()[0].scheme();
            if (auto iter = filterList.find(sch); iter == filterList.end()) {
                return;
            }

            auto path = mimeData->urls()[0].path();
            printf("drop sche: %s, path: %s\n", sch.toStdString().c_str(), path.toStdString().c_str());
            setText(path);
            setModified(true);
            emit editingFinished();
            event->accept();
        } else {
            event->ignore();
        }
    }

    PropertyUuid::PropertyUuid(void *obj, const TypeMemberNode *node, QWidget *parent) : ReflectedMemberWidget(obj, node, parent)
    {
        lineEdit = new AssetLineEditor(this);
        lineEdit->setAcceptDrops(true);
        lineEdit->setClearButtonEnabled(true);

        auto iter = node->properties.find(static_cast<uint32_t>(CommonPropertyKey::ASSET_TYPE));
        SKY_ASSERT(iter != node->properties.end());
        assetType = *iter->second.GetAsConst<std::string_view>();

        auto *button = new QPushButton(this);
        button->setText("...");
        button->setFixedWidth(32);
        layout()->addWidget(lineEdit);
        layout()->addWidget(button);

        connect(button, &QPushButton::clicked, this, [this]() {
        });

        connect(lineEdit, &QLineEdit::editingFinished, this, [this]() {
            auto *edit = qobject_cast<QLineEdit*>(sender());
            if (edit == nullptr || !edit->isModified())
            {
                return;
            }

            edit->setModified(false);
            auto input = lineEdit->text();
            auto source = AssetDataBase::Get()->FindAsset(input.toStdString());
            if (input.isEmpty()) {
                memberNode->setterFn(object, &Uuid::GetEmpty());
            } else if (source) {
                if (source->category != assetType) {
                    QMessageBox(QMessageBox::Question,
                        "Error",
                        QString("%1\nrequested: %2\ncurrent: %3")
                                .arg("Invalid Asset Type!", assetType.data(), source->category.data()),
                        QMessageBox::Ok, QApplication::activeWindow()).exec();
                } else {
                    memberNode->setterFn(object, &source->uuid);
                }
            } else {
                QMessageBox(QMessageBox::Warning,
                    "Warning",
                    "Asset Not Found.",
                    QMessageBox::Ok, QApplication::activeWindow()).exec();
            }

            RefreshValue();
        });

        RefreshValue();
    }

    void PropertyUuid::RefreshValue()
    {
        auto any = memberNode->getterConstFn(object);
        const auto *uuid = any.GetAsConst<Uuid>();
        SKY_ASSERT(uuid != nullptr);
        auto source = AssetDataBase::Get()->FindAsset(*uuid);
        lineEdit->setText(source ? source->path.path.GetStr().c_str() : "");
    }

} // namespace sky::editor
//
// Created by Zach Lee on 2026/1/11.
//

#include "AnimationNodeModel.h"
#include <editor/framework/AssetSelectWidget.h>
#include <framework/asset/AssetDataBase.h>
#include <QVBoxLayout>
#include <QPushButton>
#include <QLineEdit>
#include <QComboBox>

namespace sky::editor {

    AnimationClipEmbeddedWidget::AnimationClipEmbeddedWidget(QWidget* parent) : QWidget(parent)
    {
        QVBoxLayout *mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(0, 0, 0, 0);
        mainLayout->setSpacing(5);

        auto* selectBtn = new QPushButton("Select");
        auto* lineEdit = new QLineEdit("");
        auto* nameEdit = new QLineEdit("");

        connect(nameEdit, &QLineEdit::textChanged, this, [this](const QString& text) {
            emit OnNameChanged(text);
        });

        connect(selectBtn, &QPushButton::clicked, this, [this, lineEdit, nameEdit]() {
            AssetSelectWidget dlg("Clip");
            if (dlg.exec()) {
                auto path = dlg.GetPath();
                auto asset = AssetDataBase::Get()->FindAsset(path);
                if (asset) {
                    lineEdit->setText(path.path.GetStr().c_str());
                    emit OnAnimationAssetChanged(asset->uuid);

                    if (nameEdit->text().isEmpty()) {
                        nameEdit->setText(path.path.FileName().c_str());
                    }
                }
            }
        });

        mainLayout->addWidget(nameEdit);
        mainLayout->addWidget(selectBtn);
        mainLayout->addWidget(lineEdit);
    }

    AnimationCondFloatCompEmbeddedWidget::AnimationCondFloatCompEmbeddedWidget(QWidget* parent) : QWidget(parent)
    {
        QVBoxLayout *mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(0, 0, 0, 0);
        mainLayout->setSpacing(5);

        auto* compFunc = new QComboBox();
        compFunc->addItem("NEV");
        compFunc->addItem("LT");
        compFunc->addItem("EQ");
        compFunc->addItem("LE");
        compFunc->addItem("GT");
        compFunc->addItem("NE");
        compFunc->addItem("GE");
        compFunc->addItem("AWS");

        auto* lineEdit = new QLineEdit("");
        QDoubleValidator* validator = new QDoubleValidator(lineEdit);
        lineEdit->setValidator(validator);

        auto* nameEdit = new QLineEdit("");

        connect(compFunc, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, [this](int idx) {
            emit OnCompFuncChanged(AnimComp(idx));
        });

        connect(nameEdit, &QLineEdit::textChanged, this, [this](const QString& text) {
            emit OnSlotNameChanged(text);
        });

        connect(lineEdit, &QLineEdit::textChanged, this, [this](const QString& text) {
            emit OnParamChanged(text.toFloat());
        });

        mainLayout->addWidget(nameEdit);
        mainLayout->addWidget(compFunc);
        mainLayout->addWidget(lineEdit);
    }

    StateMachineStateNodeModel::StateMachineStateNodeModel()
        : embedded(new AnimationClipEmbeddedWidget())
    {
        connect(embedded, &AnimationClipEmbeddedWidget::OnAnimationAssetChanged, this, &StateMachineStateNodeModel::OnSelectAnimationClip);
        connect(embedded, &AnimationClipEmbeddedWidget::OnNameChanged, this, &StateMachineStateNodeModel::OnNameChanged);
    }

    void StateMachineStateNodeModel::OnSelectAnimationClip(const Uuid& id)
    {
        data = std::make_shared<AnimationClipNodeData>();
        data->clipId = id;
    }

    void StateMachineStateNodeModel::OnNameChanged(const QString& str)
    {
        if (data) {
            data->name = str;
        }
    }


    unsigned int StateMachineStateNodeModel::nPorts(QtNodes::PortType portType) const
    {
        return 4;
    }

    QtNodes::NodeDataType StateMachineStateNodeModel::dataType(QtNodes::PortType portType, QtNodes::PortIndex portIndex) const
    {
        return AnimationClipNodeData().type();
    }

    std::shared_ptr<QtNodes::NodeData> StateMachineStateNodeModel::outData(QtNodes::PortIndex port)
    {
        return data;
    }

    void StateMachineStateNodeModel::setInData(std::shared_ptr<QtNodes::NodeData> inData, QtNodes::PortIndex portIndex)
    {
        auto numberData = std::dynamic_pointer_cast<AnimationClipNodeData>(inData);

        if (!inData) {
            Q_EMIT dataInvalidated(0);
        }
    }


    StateMachineStateTransitionModel::StateMachineStateTransitionModel()
        : embedded(new AnimationCondFloatCompEmbeddedWidget())
    {
        connect(embedded, &AnimationCondFloatCompEmbeddedWidget::OnSlotNameChanged, this, &StateMachineStateTransitionModel::OnSlotNameChanged);
        connect(embedded, &AnimationCondFloatCompEmbeddedWidget::OnParamChanged, this, &StateMachineStateTransitionModel::OnParamChanged);
        connect(embedded, &AnimationCondFloatCompEmbeddedWidget::OnCompFuncChanged, this, &StateMachineStateTransitionModel::OnCompFuncChanged);

        data = std::shared_ptr<AnimationCompNodeData>();
    }

    void StateMachineStateTransitionModel::OnSlotNameChanged(const QString &str)
    {
        data->paramSlotName = str;
    }

    void StateMachineStateTransitionModel::OnParamChanged(const AnimationParam& val)
    {
        data->refVal = val;
    }

    void StateMachineStateTransitionModel::OnCompFuncChanged(AnimComp comp)
    {
        data->compFunc = comp;
    }

    unsigned int StateMachineStateTransitionModel::nPorts(QtNodes::PortType portType) const
    {
        return 1;
    }

    QtNodes::NodeDataType StateMachineStateTransitionModel::dataType(QtNodes::PortType portType, QtNodes::PortIndex portIndex) const
    {
        return AnimationClipNodeData().type();
    }

    std::shared_ptr<QtNodes::NodeData> StateMachineStateTransitionModel::outData(QtNodes::PortIndex port)
    {
        return {};
    }

    void StateMachineStateTransitionModel::setInData(std::shared_ptr<QtNodes::NodeData> inData, QtNodes::PortIndex portIndex)
    {
    }

} // namespace sky::editor
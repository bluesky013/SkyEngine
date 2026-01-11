//
// Created by Zach Lee on 2026/1/11.
//

#pragma once

#include <core/util/Uuid.h>
#include <QtNodes/NodeDelegateModel>
#include <animation/graph/AnimationState.h>
#include <render/adaptor/assets/AnimationAsset.h>

class QLineEdit;

namespace sky::editor {

    class AnimationClipNodeData : public QtNodes::NodeData {
    public:
        AnimationClipNodeData() = default;

        QtNodes::NodeDataType type() const override { return QtNodes::NodeDataType{"state", "State"}; }

        QString name;
        Uuid clipId;
    };

    class AnimationCompNodeData : public QtNodes::NodeData {
    public:
        AnimationCompNodeData() = default;

        QtNodes::NodeDataType type() const override { return QtNodes::NodeDataType{"trans", "Transition"}; }

        QString paramSlotName;
        AnimationParam refVal;
        AnimComp compFunc;
    };

    class AnimationClipEmbeddedWidget : public QWidget {
        Q_OBJECT
    public:
        explicit AnimationClipEmbeddedWidget(QWidget* parent = nullptr);
        ~AnimationClipEmbeddedWidget() override = default;

    Q_SIGNALS:
        void OnAnimationAssetChanged(const Uuid &id); // NOLINT
        void OnNameChanged(const QString& str); // NOLINT
    };

    class AnimationCondFloatCompEmbeddedWidget : public QWidget {
        Q_OBJECT
    public:
        explicit AnimationCondFloatCompEmbeddedWidget(QWidget* parent = nullptr);
        ~AnimationCondFloatCompEmbeddedWidget() override = default;

    Q_SIGNALS:
        void OnSlotNameChanged(const QString &str); // NOLINT
        void OnParamChanged(const AnimationParam& param); // NOLINT
        void OnCompFuncChanged(AnimComp comp); // NOLINT
    };


    class StateMachineStateNodeModel : public QtNodes::NodeDelegateModel {
        Q_OBJECT
    public:
        StateMachineStateNodeModel();
        ~StateMachineStateNodeModel() override = default;

        QString caption() const override { return QStringLiteral("Animation State"); }
        bool captionVisible() const override { return true; }
        QString name() const override { return QStringLiteral("State"); }

        unsigned int nPorts(QtNodes::PortType portType) const override;
        QtNodes::NodeDataType dataType(QtNodes::PortType portType, QtNodes::PortIndex portIndex) const override;
        std::shared_ptr<QtNodes::NodeData> outData(QtNodes::PortIndex port) override;
        void setInData(std::shared_ptr<QtNodes::NodeData> data, QtNodes::PortIndex portIndex) override;

        QWidget *embeddedWidget() override { return embedded; }

    public Q_SLOTS:
        void OnSelectAnimationClip(const Uuid& id);
        void OnNameChanged(const QString& str);

    private:
        std::shared_ptr<AnimationClipNodeData> data;
        AnimationClipEmbeddedWidget *embedded = nullptr;
    };

    class StateMachineStateTransitionModel : public QtNodes::NodeDelegateModel {
        Q_OBJECT
    public:
        StateMachineStateTransitionModel();
        ~StateMachineStateTransitionModel() override = default;

        QString caption() const override { return QStringLiteral("Animation Condition"); }
        bool captionVisible() const override { return false; }
        QString name() const override { return QStringLiteral("FloatCondition"); }

        unsigned int nPorts(QtNodes::PortType portType) const override;
        QtNodes::NodeDataType dataType(QtNodes::PortType portType, QtNodes::PortIndex portIndex) const override;
        std::shared_ptr<QtNodes::NodeData> outData(QtNodes::PortIndex port) override;
        void setInData(std::shared_ptr<QtNodes::NodeData> data, QtNodes::PortIndex portIndex) override;

        QWidget *embeddedWidget() override { return embedded; }

    private:
        void OnSlotNameChanged(const QString &str);
        void OnParamChanged(const AnimationParam& param);
        void OnCompFuncChanged(AnimComp comp);

        std::shared_ptr<AnimationCompNodeData> data;
        AnimationCondFloatCompEmbeddedWidget* embedded = nullptr;
    };


} // namespace sky::editor
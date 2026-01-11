//
// Created by Zach Lee on 2026/1/11.
//

#pragma once

#include <QtNodes/NodeDelegateModel>
#include <animation/graph/AnimationState.h>

namespace sky::editor {

    class AnimationStateData : public QtNodes::NodeData {
    public:
        AnimationStateData() = default;

        QtNodes::NodeDataType type() const override { return QtNodes::NodeDataType{"animState", "AnimState"}; }
    };

    class StateMachineStateNodeModel : public QtNodes::NodeDelegateModel {
        Q_OBJECT
    public:
        StateMachineStateNodeModel() = default;
        ~StateMachineStateNodeModel() override = default;

        unsigned int nPorts(QtNodes::PortType portType) const override;
        QtNodes::NodeDataType dataType(QtNodes::PortType portType, QtNodes::PortIndex portIndex) const override;
        std::shared_ptr<QtNodes::NodeData> outData(QtNodes::PortIndex port) override;
        void setInData(std::shared_ptr<QtNodes::NodeData> data, QtNodes::PortIndex portIndex) override;

        QWidget *embeddedWidget() override { return nullptr; }
    };


} // namespace sky::editor
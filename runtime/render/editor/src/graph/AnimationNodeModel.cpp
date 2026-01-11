//
// Created by Zach Lee on 2026/1/11.
//

#include "AnimationNodeModel.h"

namespace sky::editor {

    unsigned int StateMachineStateNodeModel::nPorts(QtNodes::PortType portType) const
    {
        return {};
    }

    QtNodes::NodeDataType StateMachineStateNodeModel::dataType(QtNodes::PortType portType, QtNodes::PortIndex portIndex) const
    {
        return {};
    }

    std::shared_ptr<QtNodes::NodeData> StateMachineStateNodeModel::outData(QtNodes::PortIndex port)
    {
        return {};
    }

    void StateMachineStateNodeModel::setInData(std::shared_ptr<QtNodes::NodeData> data, QtNodes::PortIndex portIndex)
    {

    }

} // namespace sky::editor
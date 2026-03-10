//
// Created by blues on 2026/3/10.
//

#include "ShaderGraphNodeModel.h"
#include <shader/shadergraph/ShaderGraphMathNodes.h>
#include <shader/shadergraph/ShaderGraphInputNodes.h>
#include <shader/shadergraph/ShaderGraphOutputNode.h>
#include <QVBoxLayout>
#include <QLabel>
#include <algorithm>

namespace sky::editor {

    // ---- SGNodeModel ----

    SGNodeModel::SGNodeModel(sg::SGNodePtr n) : node(std::move(n))
    {
    }

    QString SGNodeModel::caption() const
    {
        return QString::fromStdString(node->GetDisplayName());
    }

    QString SGNodeModel::name() const
    {
        return QString::fromStdString(node->GetTypeName());
    }

    unsigned int SGNodeModel::nPorts(QtNodes::PortType portType) const
    {
        if (portType == QtNodes::PortType::In) {
            return static_cast<unsigned int>(node->GetInputPins().size());
        }
        return static_cast<unsigned int>(node->GetOutputPins().size());
    }

    QtNodes::NodeDataType SGNodeModel::dataType(QtNodes::PortType portType,
                                                 QtNodes::PortIndex portIndex) const
    {
        const auto* pins = (portType == QtNodes::PortType::In)
                            ? &node->GetInputPins()
                            : &node->GetOutputPins();

        if (portIndex < static_cast<int>(pins->size())) {
            const auto& pin = (*pins)[portIndex];
            return {sg::SGDataTypeToString(pin.type).c_str(),
                    pin.name.c_str()};
        }
        return {"float", "Value"};
    }

    std::shared_ptr<QtNodes::NodeData> SGNodeModel::outData(QtNodes::PortIndex port)
    {
        const auto& outPins = node->GetOutputPins();
        if (port < static_cast<int>(outPins.size())) {
            return std::make_shared<SGNodeData>(outPins[port].type);
        }
        return nullptr;
    }

    void SGNodeModel::setInData(std::shared_ptr<QtNodes::NodeData> /*data*/,
                                 QtNodes::PortIndex /*portIndex*/)
    {
        // Data flow is handled at graph level; nothing to update here
    }

    // ---- SGConstantFloatNodeModel ----

    SGConstantFloatNodeModel::SGConstantFloatNodeModel()
        : SGNodeModel(std::make_shared<sg::SGConstantFloatNode>())
    {
    }

    QWidget* SGConstantFloatNodeModel::embeddedWidget()
    {
        if (!spinBox) {
            spinBox = new QDoubleSpinBox();
            spinBox->setRange(-1e6, 1e6);
            spinBox->setDecimals(4);
            spinBox->setValue(0.0);
            spinBox->setFixedWidth(100);

            connect(spinBox, static_cast<void(QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
                    this, [this](double v) {
                        auto* n = static_cast<sg::SGConstantFloatNode*>(node.get());
                        n->SetValue(static_cast<float>(v));
                        emit OnValueChanged(v);
                    });
        }
        return spinBox;
    }

    // ---- SGConstantVec3NodeModel ----

    SGConstantVec3NodeModel::SGConstantVec3NodeModel()
        : SGNodeModel(std::make_shared<sg::SGConstantVec3Node>())
    {
    }

    QWidget* SGConstantVec3NodeModel::embeddedWidget()
    {
        if (!container) {
            container = new QWidget();
            auto* layout = new QVBoxLayout(container);
            layout->setContentsMargins(0, 0, 0, 0);
            layout->setSpacing(2);

            spinX = new QDoubleSpinBox();
            spinY = new QDoubleSpinBox();
            spinZ = new QDoubleSpinBox();

            for (auto* spin : {spinX, spinY, spinZ}) {
                spin->setRange(-1e6, 1e6);
                spin->setDecimals(4);
                spin->setValue(0.0);
                spin->setFixedWidth(100);
                layout->addWidget(spin);
            }

            auto updateNode = [this]() {
                auto* n = static_cast<sg::SGConstantVec3Node*>(node.get());
                n->SetValue(static_cast<float>(spinX->value()),
                            static_cast<float>(spinY->value()),
                            static_cast<float>(spinZ->value()));
            };

            connect(spinX, static_cast<void(QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
                    this, [updateNode](double) { updateNode(); });
            connect(spinY, static_cast<void(QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
                    this, [updateNode](double) { updateNode(); });
            connect(spinZ, static_cast<void(QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
                    this, [updateNode](double) { updateNode(); });

            container->setLayout(layout);
        }
        return container;
    }

    // ---- SGScalarParamNodeModel ----

    SGScalarParamNodeModel::SGScalarParamNodeModel()
        : SGNodeModel(std::make_shared<sg::SGScalarParamNode>())
    {
    }

    QWidget* SGScalarParamNodeModel::embeddedWidget()
    {
        if (!nameEdit) {
            nameEdit = new QLineEdit("Param");
            nameEdit->setFixedWidth(100);
            connect(nameEdit, &QLineEdit::textChanged, this, [this](const QString& text) {
                auto* n = static_cast<sg::SGScalarParamNode*>(node.get());
                n->SetParamName(text.toStdString());
            });
        }
        return nameEdit;
    }

    // ---- SGVectorParamNodeModel ----

    SGVectorParamNodeModel::SGVectorParamNodeModel()
        : SGNodeModel(std::make_shared<sg::SGVectorParamNode>())
    {
    }

    QWidget* SGVectorParamNodeModel::embeddedWidget()
    {
        if (!nameEdit) {
            nameEdit = new QLineEdit("VecParam");
            nameEdit->setFixedWidth(100);
            connect(nameEdit, &QLineEdit::textChanged, this, [this](const QString& text) {
                auto* n = static_cast<sg::SGVectorParamNode*>(node.get());
                n->SetParamName(text.toStdString());
            });
        }
        return nameEdit;
    }

    // ---- SGTextureParamNodeModel ----

    SGTextureParamNodeModel::SGTextureParamNodeModel()
        : SGNodeModel(std::make_shared<sg::SGTextureParamNode>())
    {
    }

    QWidget* SGTextureParamNodeModel::embeddedWidget()
    {
        if (!nameEdit) {
            nameEdit = new QLineEdit("Texture");
            nameEdit->setFixedWidth(100);
            connect(nameEdit, &QLineEdit::textChanged, this, [this](const QString& text) {
                auto* n = static_cast<sg::SGTextureParamNode*>(node.get());
                n->SetParamName(text.toStdString());
            });
        }
        return nameEdit;
    }

} // namespace sky::editor

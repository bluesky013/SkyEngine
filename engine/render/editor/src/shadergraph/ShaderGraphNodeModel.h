//
// Created by blues on 2026/3/10.
//

#pragma once

#include <shader/shadergraph/ShaderGraphNode.h>
#include <shader/shadergraph/ShaderGraphTypes.h>
#include <QtNodes/NodeDelegateModel>
#include <QWidget>
#include <QLabel>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <memory>
#include <string>

namespace sky::editor {

    // ---- NodeData types ----

    // Wraps a shader graph data type for QtNodes connections
    class SGNodeData : public QtNodes::NodeData {
    public:
        explicit SGNodeData(sg::SGDataType type) : dataType(type) {}

        QtNodes::NodeDataType type() const override
        {
            return {sg::SGDataTypeToString(dataType).c_str(),
                    sg::SGDataTypeToHLSL(dataType).c_str()};
        }

        sg::SGDataType GetSGDataType() const { return dataType; }

    private:
        sg::SGDataType dataType;
    };

    // ---- Base node model ----

    // Wraps an SGNode to make it compatible with the QtNodes data-flow graph
    class SGNodeModel : public QtNodes::NodeDelegateModel {
        Q_OBJECT
    public:
        explicit SGNodeModel(sg::SGNodePtr node);
        ~SGNodeModel() override = default;

        QString caption() const override;
        bool captionVisible() const override { return true; }
        QString name() const override;

        unsigned int nPorts(QtNodes::PortType portType) const override;
        QtNodes::NodeDataType dataType(QtNodes::PortType portType, QtNodes::PortIndex portIndex) const override;
        std::shared_ptr<QtNodes::NodeData> outData(QtNodes::PortIndex port) override;
        void setInData(std::shared_ptr<QtNodes::NodeData> data, QtNodes::PortIndex portIndex) override;

        QWidget* embeddedWidget() override { return nullptr; }

        sg::SGNodePtr GetNode() const { return node; }

    protected:
        sg::SGNodePtr node;
    };

    // Template helper: wraps any SGNode subtype as a default-constructible model
    template <typename SGNodeType>
    class SGConcreteNodeModel : public SGNodeModel {
    public:
        SGConcreteNodeModel() : SGNodeModel(std::make_shared<SGNodeType>()) {}
        ~SGConcreteNodeModel() override = default;
    };

    // ---- Constant float node model ----

    class SGConstantFloatNodeModel : public SGNodeModel {
        Q_OBJECT
    public:
        SGConstantFloatNodeModel();
        ~SGConstantFloatNodeModel() override = default;

        QString name() const override { return QStringLiteral("ConstantFloat"); }

        QWidget* embeddedWidget() override;

    Q_SIGNALS:
        void OnValueChanged(double value); // NOLINT

    private:
        QDoubleSpinBox* spinBox = nullptr;
    };

    // ---- Constant Vec3 node model ----

    class SGConstantVec3NodeModel : public SGNodeModel {
        Q_OBJECT
    public:
        SGConstantVec3NodeModel();
        ~SGConstantVec3NodeModel() override = default;

        QString name() const override { return QStringLiteral("ConstantVec3"); }

        QWidget* embeddedWidget() override;

    private:
        QWidget*        container = nullptr;
        QDoubleSpinBox* spinX = nullptr;
        QDoubleSpinBox* spinY = nullptr;
        QDoubleSpinBox* spinZ = nullptr;
    };

    // ---- Scalar parameter node model ----

    class SGScalarParamNodeModel : public SGNodeModel {
        Q_OBJECT
    public:
        SGScalarParamNodeModel();
        ~SGScalarParamNodeModel() override = default;

        QString name() const override { return QStringLiteral("ScalarParam"); }

        QWidget* embeddedWidget() override;

    private:
        QLineEdit* nameEdit = nullptr;
    };

    // ---- Vector parameter node model ----

    class SGVectorParamNodeModel : public SGNodeModel {
        Q_OBJECT
    public:
        SGVectorParamNodeModel();
        ~SGVectorParamNodeModel() override = default;

        QString name() const override { return QStringLiteral("VectorParam"); }

        QWidget* embeddedWidget() override;

    private:
        QLineEdit* nameEdit = nullptr;
    };

    // ---- Texture parameter node model ----

    class SGTextureParamNodeModel : public SGNodeModel {
        Q_OBJECT
    public:
        SGTextureParamNodeModel();
        ~SGTextureParamNodeModel() override = default;

        QString name() const override { return QStringLiteral("TextureParam"); }

        QWidget* embeddedWidget() override;

    private:
        QLineEdit* nameEdit = nullptr;
    };

} // namespace sky::editor


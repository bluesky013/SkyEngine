//
// Created by blues on 2026/3/10.
//

#include "ShaderGraphWidget.h"
#include "ShaderGraphNodeModel.h"

#include <shader/shadergraph/ShaderGraphMathNodes.h>
#include <shader/shadergraph/ShaderGraphInputNodes.h>
#include <shader/shadergraph/ShaderGraphOutputNode.h>

#include <QVBoxLayout>
#include <QtNodes/NodeDelegateModelRegistry>

namespace sky::editor {

    // Register all shader graph node models for the visual editor
    static std::shared_ptr<QtNodes::NodeDelegateModelRegistry> RegisterShaderGraphModels()
    {
        auto ret = std::make_shared<QtNodes::NodeDelegateModelRegistry>();

        // Math
        ret->registerModel<SGConcreteNodeModel<sg::SGAddNode>>("Math");
        ret->registerModel<SGConcreteNodeModel<sg::SGSubtractNode>>("Math");
        ret->registerModel<SGConcreteNodeModel<sg::SGMultiplyNode>>("Math");
        ret->registerModel<SGConcreteNodeModel<sg::SGDivideNode>>("Math");
        ret->registerModel<SGConcreteNodeModel<sg::SGLerpNode>>("Math");
        ret->registerModel<SGConcreteNodeModel<sg::SGClampNode>>("Math");
        ret->registerModel<SGConcreteNodeModel<sg::SGSaturateNode>>("Math");
        ret->registerModel<SGConcreteNodeModel<sg::SGAbsNode>>("Math");
        ret->registerModel<SGConcreteNodeModel<sg::SGPowerNode>>("Math");
        ret->registerModel<SGConcreteNodeModel<sg::SGDotNode>>("Math");
        ret->registerModel<SGConcreteNodeModel<sg::SGCrossNode>>("Math");
        ret->registerModel<SGConcreteNodeModel<sg::SGNormalizeNode>>("Math");
        ret->registerModel<SGConcreteNodeModel<sg::SGComponentMaskNode>>("Math");
        ret->registerModel<SGConcreteNodeModel<sg::SGAppendNode>>("Math");

        // Input
        ret->registerModel<SGConcreteNodeModel<sg::SGTexCoordNode>>("Input");
        ret->registerModel<SGConcreteNodeModel<sg::SGVertexColorNode>>("Input");
        ret->registerModel<SGConcreteNodeModel<sg::SGWorldPositionNode>>("Input");
        ret->registerModel<SGConcreteNodeModel<sg::SGWorldNormalNode>>("Input");
        ret->registerModel<SGConcreteNodeModel<sg::SGTimeNode>>("Input");

        // Constants
        ret->registerModel<SGConstantFloatNodeModel>("Constant");
        ret->registerModel<SGConcreteNodeModel<sg::SGConstantVec2Node>>("Constant");
        ret->registerModel<SGConstantVec3NodeModel>("Constant");
        ret->registerModel<SGConcreteNodeModel<sg::SGConstantVec4Node>>("Constant");

        // Parameters
        ret->registerModel<SGScalarParamNodeModel>("Parameter");
        ret->registerModel<SGVectorParamNodeModel>("Parameter");
        ret->registerModel<SGTextureParamNodeModel>("Parameter");

        // Texture
        ret->registerModel<SGConcreteNodeModel<sg::SGTextureSampleNode>>("Texture");

        // Output
        ret->registerModel<SGConcreteNodeModel<sg::SGMaterialOutputNode>>("Output");

        return ret;
    }

    ShaderGraphWidget::ShaderGraphWidget(const FilePtr& source)
        : registry(RegisterShaderGraphModels())
        , model(new QtNodes::DataFlowGraphModel(registry))
        , scene(new QtNodes::DataFlowGraphicsScene(*model))
        , view(new QtNodes::GraphicsView(scene))
        , asset(source)
    {
        auto* mainLayout = new QVBoxLayout(this);
        mainLayout->addWidget(view);
        setLayout(mainLayout);
        setBaseSize(1200, 800);

        connect(scene, &QtNodes::DataFlowGraphicsScene::modified, this, [this]() {
            setWindowModified(true);
        });
    }

} // namespace sky::editor

//
// Created by blues on 2024/11/30.
//

#include <terrain/editor/TerrainToolWidget.h>
#include <QVBoxLayout>
#include <QTabWidget>
#include <QButtonGroup>
#include <QRadioButton>
#include <core/shapes/Base.h>
#include <core/shapes/Shapes.h>
#include <framework/window/NativeWindowManager.h>
#include <framework/world/TransformComponent.h>
#include <framework/asset/AssetDataBase.h>
#include <editor/framework/WorldTreeView.h>
#include <editor/framework/EditorCamera.h>
#include <render/adaptor/RenderSceneProxy.h>
#include <terrain/TerrainComponent.h>
#include <terrain/TerrainUtils.h>

namespace sky::editor {
    TerrainGenerateTool::TerrainGenerateTool(QWidget *parent) : TerrainToolBase(parent)
    {
        auto *layout = new QVBoxLayout(this);
        setLayout(layout);

        config = new TObjectWidget<TerrainGenerateConfig>(this);
        layout->addWidget(config);

        generateBtn = new QPushButton("Generate Terrain");
        layout->addWidget(generateBtn);
    }

    const TerrainGenerateConfig &TerrainGenerateTool::GetConfig() const
    {
        return config->GetValue();
    }

    void TerrainGenerateTool::OnDraw(TerrainHelper &helper, TerrainComponent* comp)
    {
        if (comp == nullptr) {
            return;
        }

        auto terrainPos = comp->GetActor()->GetComponent<TransformComponent>()->GetWorldTransform().translation;
        const auto &terrainData = comp->GetData();
        helper.DrawTerrainBound(terrainData, terrainPos);
    }

    TerrainGridTool::TerrainGridTool(QWidget* parent) : TerrainToolBase(parent)
    {
        auto *layout = new QVBoxLayout(this);
        setLayout(layout);

        group = new QButtonGroup(this);
        auto *add = new QRadioButton("Add");
        auto *del = new QRadioButton("Del");
        group->addButton(add);
        group->addButton(del);
        add->click();

        layout->addWidget(add);
        layout->addWidget(del);
    }

    void TerrainGridTool::OnDraw(TerrainHelper &helper, TerrainComponent *comp)
    {
        if (comp == nullptr) {
            return;
        }

        auto terrainPos = comp->GetActor()->GetComponent<TransformComponent>()->GetWorldTransform().translation;
        const auto &terrainData = comp->GetData();
        helper.DrawSelectedGrid(terrainData, selectX, selectY, terrainPos);
    }

    void TerrainGridTool::OnMouseMotion(TerrainHelper &helper, const MouseMotionEvent &eve, TerrainComponent *comp, EditorCamera *cam)
    {
        if (comp == nullptr) {
            return;
        }

        auto terrainPos = comp->GetActor()->GetComponent<TransformComponent>()->GetWorldTransform().translation;

        auto screenWidth = static_cast<float>(cam->GetScreenWidth());
        auto screenHeight = static_cast<float>(cam->GetScreenHeight());
        Vector2 screenPos(static_cast<float>(eve.x) / screenWidth, static_cast<float>(eve.y) / screenHeight);
        Vector2 ndc = screenPos * 2.f - Vector2(1.f, 1.f);

        auto camPos = ToVec3(cam->GetWorldMatrix().m[3]);
        auto dst = cam->GetViewProjectMatrix().Inverse() * Vector4(ndc.x, ndc.y, 0.5f, 1.f);
        auto dstPos = Vector3(dst.x / dst.w, dst.y / dst.w, dst.z / dst.w);

        auto dir = dstPos - camPos; dir.Normalize();
        auto ray = Ray{camPos, dir};
        auto plane = Plane{VEC3_Y, terrainPos.y};

        selectX = -1;
        selectY = -1;

        auto res = CalculateInterSection(ray, plane);
        if (res.first) {
            const auto &terrainData = comp->GetData();
            auto size = ConvertSectionSize(terrainData.sectionSize);

            auto local = res.second - terrainPos;
            auto x = static_cast<int32_t>(std::floor(local.x / terrainData.resolution / static_cast<float>(size)));
            auto y = static_cast<int32_t>(std::floor(local.z / terrainData.resolution / static_cast<float>(size)));

            if ((x >= 0 && x < terrainData.sectionBoundX) && (y >= 0 && y < terrainData.sectionBoundY)) {
                selectX = x;
                selectY = y;
            }
            helper.DrawSelectedGrid(terrainData, x, y, terrainPos);
        }

    }

    void TerrainGridTool::OnMouseUp(TerrainHelper &helper, const MouseButtonEvent &eve, TerrainComponent *comp, EditorCamera *cam)
    {
        if (comp == nullptr) {
            return;
        }

        auto mode = group->checkedButton()->text();
        bool addMode = mode == "Add";

        if (selectX != -1 && selectY != -1 && eve.button == MouseButtonType::LEFT) {
            if (addMode) {
                comp->AddSection(selectX, selectY);
            } else {
                comp->RemoveSection(selectX, selectY);
            }

            auto terrainPos = comp->GetActor()->GetComponent<TransformComponent>()->GetWorldTransform().translation;
            const auto &terrainData = comp->GetData();
            helper.DrawSelectedGrid(terrainData, selectX, selectY, terrainPos);
        }
    }

    TerrainBuildTool::TerrainBuildTool(QWidget *parent) : TerrainToolBase(parent)
    {
        auto *layout = new QVBoxLayout(this);
        setLayout(layout);

        config = new TObjectWidget<TerrainBuildConfig>(this);
        layout->addWidget(config);

        createBtn = new QPushButton("Create Terrain");
        layout->addWidget(createBtn);
    }

    void TerrainBuildTool::OnDraw(TerrainHelper &helper, TerrainComponent *component)
    {
        helper.DrawFullTerrainGrid(config->GetValue(), VEC3_ZERO);
    }

    const TerrainBuildConfig &TerrainBuildTool::GetConfig() const
    {
        return config->GetValue();
    }

    TerrainToolWidget::TerrainToolWidget(QWidget* parent)
        : ToolWidget(parent)
    {
        auto *layout = new QVBoxLayout(this);
        setLayout(layout);

        tabWidget = new QTabWidget(this);
        layout->addWidget(tabWidget);

        buildTool = new TerrainBuildTool(this);
        tabWidget->addTab(buildTool, "Build");

        gridTool = new TerrainGridTool(this);
        tabWidget->addTab(gridTool, "Grid");

        generateTool = new TerrainGenerateTool(this);
        tabWidget->addTab(generateTool, "Generate");

        connect(tabWidget, &QTabWidget::currentChanged, this, &TerrainToolWidget::OnModeChanged);
        connect(buildTool->createBtn, &QPushButton::clicked, this, &TerrainToolWidget::OnCreateTerrain);
        connect(generateTool->generateBtn, &QPushButton::clicked, this, &TerrainToolWidget::OnGenerateTerrain);

        helper = std::make_unique<TerrainHelper>();

        mouseBinder.Bind(this);
        selectBinder.Bind(this);
    }

    TerrainToolBase *TerrainToolWidget::GetActiveTool()
    {
        return static_cast<TerrainToolBase *>(tabWidget->currentWidget());
    }

    void TerrainToolWidget::OnModeChanged(int index)
    {
        static_cast<TerrainToolBase *>(tabWidget->currentWidget())->OnDraw(*helper, terrainComponent);
    }

    void TerrainToolWidget::OnCreateTerrain()
    {
//        if (world != nullptr) {
//            auto selectActor = world->AddActor("Terrain");
//            terrainComponent = selectActor->AddComponent<TerrainComponent>();
//            terrainComponent->BuildTerrain(buildTool->GetConfig());
//        }
    }

    void TerrainToolWidget::OnGenerateTerrain()
    {
        if (terrainComponent != nullptr) {
            generator.Run(terrainComponent);
        }
    }

    void TerrainToolWidget::OnSetWorld()
    {
        if (renderScene != nullptr) {
            for (auto *prim : helper->GetPrimitives()) {
                renderScene->RemovePrimitive(prim);
            }
        }

        renderScene = nullptr;
        terrainComponent = nullptr;
        if (world != nullptr) {
            renderScene = static_cast<RenderSceneProxy *>(world->GetWorld()->GetSubSystem(Name("RenderScene")))->GetRenderScene();
            for (auto *prim : helper->GetPrimitives()) {
                renderScene->AddPrimitive(prim);
            }
        }
    }

    void TerrainToolWidget::Activate(WindowID winId)
    {
        activeWindow = winId;
        OnModeChanged(0);
    }

    void TerrainToolWidget::DeActivate()
    {
        activeWindow = 0;
        helper->Reset();
    }

#define FILTER_WIN_ID(id)    \
    if (id != activeWindow) {\
        return;              \
    }
    void TerrainToolWidget::OnMouseButtonDown(const MouseButtonEvent &event)
    {
        FILTER_WIN_ID(event.winID)
        GetActiveTool()->OnMouseDown(*helper, event, terrainComponent, camera);
    }

    void TerrainToolWidget::OnMouseButtonUp(const MouseButtonEvent &event)
    {
        FILTER_WIN_ID(event.winID)
        GetActiveTool()->OnMouseUp(*helper, event, terrainComponent, camera);
    }

    void TerrainToolWidget::OnMouseMotion(const MouseMotionEvent &event)
    {
        FILTER_WIN_ID(event.winID)
        GetActiveTool()->OnMouseMotion(*helper, event, terrainComponent, camera);
    }

    void TerrainToolWidget::OnActorSelected(Actor *actor)
    {
        if (actor != nullptr) {
            terrainComponent = actor->GetComponent<TerrainComponent>();
        } else {
            terrainComponent = nullptr;
        }
    }
} // namespace sky::editor

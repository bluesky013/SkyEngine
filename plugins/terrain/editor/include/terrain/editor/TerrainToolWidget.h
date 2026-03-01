//
// Created by blues on 2024/11/30.
//

#pragma once

#include <editor/framework/EditorToolBase.h>
#include <editor/framework/ReflectedObjectWidget.h>
#include <terrain/TerrainBase.h>
#include <terrain/editor/TerrainToolHelper.h>
#include <terrain/editor/TerrainGenerator.h>

#include <framework/world/World.h>
#include <framework/interface/ISelectEvent.h>

class QTabWidget;
class QButtonGroup;

namespace sky {
    class RenderScene;
    class TerrainComponent;
} // namespace sky

namespace sky::editor {
    class WorldTreeView;
    class EditorCamera;

    class TerrainToolBase : public QWidget {
    public:
        explicit TerrainToolBase(QWidget *parent) : QWidget(parent) {}
        ~TerrainToolBase() override = default;

        virtual void OnDraw(TerrainHelper &helper, TerrainComponent*) {}
        virtual void OnMouseMotion(TerrainHelper &helper, const MouseMotionEvent &eve, TerrainComponent*, EditorCamera*) {}
        virtual void OnMouseUp(TerrainHelper &helper, const MouseButtonEvent &eve, TerrainComponent*, EditorCamera*) {}
        virtual void OnMouseDown(TerrainHelper &helper, const MouseButtonEvent &eve, TerrainComponent*, EditorCamera*) {}
    };

    class TerrainGenerateTool : public TerrainToolBase {
    public:
        explicit TerrainGenerateTool(QWidget *parent);
        ~TerrainGenerateTool() override = default;
        const TerrainGenerateConfig &GetConfig() const;

        QPushButton *generateBtn = nullptr;
    private:
        void OnDraw(TerrainHelper &helper, TerrainComponent *) override;
        TObjectWidget<TerrainGenerateConfig> *config = nullptr;
    };

    class TerrainGridTool : public TerrainToolBase {
    public:
        explicit TerrainGridTool(QWidget *parent);
        ~TerrainGridTool() override = default;

    private:
        void OnDraw(TerrainHelper &helper, TerrainComponent *) override;
        void OnMouseMotion(TerrainHelper &helper, const MouseMotionEvent &eve, TerrainComponent *, EditorCamera *) override;
        void OnMouseUp(TerrainHelper &helper, const MouseButtonEvent &eve, TerrainComponent *, EditorCamera *) override;

        int32_t selectX = -1;
        int32_t selectY = -1;

        QButtonGroup* group = nullptr;
    };

    class TerrainBuildTool : public TerrainToolBase {
    public:
        explicit TerrainBuildTool(QWidget* parent);
        ~TerrainBuildTool() override = default;
        const TerrainBuildConfig &GetConfig() const;
        
        void OnDraw(TerrainHelper &helper, TerrainComponent *) override;

        QPushButton *createBtn = nullptr;
    private:
        ActorPtr selectActor;
        TObjectWidget<TerrainBuildConfig> *config = nullptr;
    };

    class TerrainToolWidget
        : public ToolWidget
        , public IMouseEvent
        , public ISelectEvent
    {
    public:
        explicit TerrainToolWidget(QWidget* parent);
        ~TerrainToolWidget() override = default;

    private:
        TerrainToolBase *GetActiveTool();

        void Activate(WindowID winId) override;
        void DeActivate() override;

        void OnSetWorld() override;
        void OnModeChanged(int index);

        void OnCreateTerrain();
        void OnGenerateTerrain();

        void OnMouseButtonDown(const MouseButtonEvent &event) override;
        void OnMouseButtonUp(const MouseButtonEvent &event) override;
        void OnMouseMotion(const MouseMotionEvent &event) override;

        void OnActorSelected(Actor *actor) override;

        EventBinder<IMouseEvent> mouseBinder;
        EventBinder<ISelectEvent> selectBinder;

        WindowID activeWindow = 0;
        QTabWidget *tabWidget = nullptr;
        TerrainBuildTool *buildTool = nullptr;
        TerrainGridTool *gridTool = nullptr;
        TerrainGenerateTool *generateTool = nullptr;

        TerrainComponent* terrainComponent = nullptr;
        RenderScene *renderScene = nullptr;
        std::unique_ptr<TerrainHelper> helper;

        TerrainGenerator generator;
    };

} // namespace sky::editor
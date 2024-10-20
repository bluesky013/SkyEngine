//
// Created by blues on 2024/3/17.
//

#include <editor/EditorGuiInstance.h>
#include <editor/widgets/DemoWidget.h>
#include <render/adaptor/RenderSceneProxy.h>
#include <imgui/ImGuiFeatureProcessor.h>
//#include <editor/widgets/WorldWidget.h>
//#include <editor/widgets/InspectorWidget.h>
//#include <framework/asset/AssetManager.h>

namespace sky::editor {

    EditorGuiInstance::~EditorGuiInstance()
    {
        if (gui) {
            guiInstance->RemoveWidget(gui.get());
        }
    }

    void EditorGuiInstance::Init(World &world, NativeWindow* window)
    {
        renderScene = static_cast<RenderSceneProxy*>(world.GetSubSystem("RenderScene"))->GetRenderScene();
        auto *imguiFeature = renderScene->GetFeature<ImGuiFeatureProcessor>();
        if (imguiFeature != nullptr) {
            guiInstance = imguiFeature->CreateImGuiInstance();
            guiInstance->BindNativeWindow(window);
            renderScene->AddPrimitive(guiInstance->GetPrimitive());
            Init();
        }
    }

    void EditorGuiInstance::Init()
    {
//        wm = std::make_unique<WidgetManager>();

//        menuBar = new MainMenuBar();
//        wm->RegisterWidget(menuBar);

        // menu
//        auto *fileMenu = menuBar->AddMenu("File");
//        fileMenu->AddItem("Import", BTN_MENU_FILE_IMPORT);
//
//        auto *viewMenu = menuBar->AddMenu("View");
//        viewMenu->AddItem("Assets", BTN_MENU_VIEW_SHOW_ASSETS, false);
//        viewMenu->AddItem("Demo", BTN_MENU_VIEW_SHOW_DEMO, false);
//        viewMenu->AddItem("World", BTN_MENU_VIEW_SHOW_WORLD, false);
//
//        auto *demoWidget = new DemoWidget();
//        demoWidget->BindEvent(BTN_MENU_VIEW_SHOW_DEMO);
//
//        auto *assetWidget = new AssetWidget();
//        assetWidget->BindEvent(BTN_MENU_VIEW_SHOW_ASSETS);
//
//        worldWidget = new WorldWidget();
//        worldWidget->BindEvent(BTN_MENU_VIEW_SHOW_WORLD);
//
//        auto *inspect = new InspectorWidget();
//        inspect->BindEvent(BTN_MENU_VIEW_SHOW_WORLD);

//        auto *dirWidget = new DirectoryBrowser();
//        dirWidget->AddPath(AssetManager::Get()->GetProjectAssetPath());
//        dirWidget->AddPath(AssetManager::Get()->GetEngineAssetPath());
//        dirWidget->BindEvent(BTN_MENU_FILE_IMPORT);
//
//        wm->RegisterWidget(demoWidget);
//        wm->RegisterWidget(worldWidget);
//        wm->RegisterWidget(inspect);

        gui = std::make_unique<GuiZmoWidget>();
        gui->AttachScene(renderScene);

//        instance->AddWidget(wm.get());
        guiInstance->AddWidget(gui.get());
    }
} // namespace sky::editor
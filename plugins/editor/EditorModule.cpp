//
// Created by Zach on 2024/3/17.
//

#include <editor/EditorInstance.h>
#include <imgui/ImGuiFeature.h>
#include <framework/interface/IModule.h>
#include <framework/interface/Interface.h>
#include <framework/interface/ISystem.h>

namespace sky::editor {

    class EditorModule : public IModule {
    public:
        EditorModule() = default;
        ~EditorModule() override = default;

        void Shutdown() override;
        void Start() override;
        void Tick(float delta) override;

    private:
        ImGuiInstance *guiInstance = nullptr;
    };

    void EditorModule::Shutdown()
    {
        EditorInstance::Destroy();
    }
    void EditorModule::Start()
    {
        const auto *nativeWindow = Interface<ISystemNotify>::Get()->GetApi()->GetViewport();
        if (nativeWindow != nullptr) {
            guiInstance = ImGuiFeature::Get()->GetGuiInstance();
            guiInstance->MakeCurrent();
            guiInstance->BindNativeWindow(nativeWindow);
            EditorInstance::Get()->Init(guiInstance);
        }
    }
    void EditorModule::Tick(float delta)
    {
        guiInstance->Tick(delta);
    }
} // namespace sky
REGISTER_MODULE(sky::editor::EditorModule)
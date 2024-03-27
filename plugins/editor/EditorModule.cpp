//
// Created by Zach on 2024/3/17.
//

#include <framework/interface/IModule.h>
#include <editor/EditorInstance.h>

namespace sky::editor {

    class EditorModule : public IModule {
    public:
        EditorModule() = default;
        ~EditorModule() override = default;

        void Shutdown() override;
        void Start() override;
        void Tick(float delta) override;
    };

    void EditorModule::Shutdown()
    {
        EditorInstance::Destroy();
    }
    void EditorModule::Start()
    {
        EditorInstance::Get()->Init();
    }
    void EditorModule::Tick(float delta)
    {
        EditorInstance::Get()->Tick(delta);
    }
} // namespace sky
REGISTER_MODULE(sky::editor::EditorModule)
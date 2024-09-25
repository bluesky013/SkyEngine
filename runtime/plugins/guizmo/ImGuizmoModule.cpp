//
// Created by Zach on 2024/3/17.
//

#include <framework/interface/IModule.h>
#include <framework/interface/Interface.h>
#include <framework/interface/IGizmo.h>
#include <editor/EditorGuiInstance.h>

namespace sky::editor {

    class ImGuizmoModule : public IModule, public IGizmoFactory {
    public:
        ImGuizmoModule()
        {
            Interface<IGizmoFactory>::Get()->Register(*this);
        }

        ~ImGuizmoModule() override
        {
            Interface<IGizmoFactory>::Get()->UnRegister();
        }

        IGizmo* CreateGizmo() override
        {
            return new EditorGuiInstance();
        }
    };
} // namespace sky
REGISTER_MODULE(sky::editor::ImGuizmoModule)
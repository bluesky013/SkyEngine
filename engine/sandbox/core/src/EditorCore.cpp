//
// Created on 2026/09/21.
//
// Module entry point for EditorCore. The editor services (undo/redo, property
// model, documents, selection) live under this module. It must stay free of
// Aurora, the legacy render stack, the UI layer, and Qt (enforced by a
// configure-time guard in CMakeLists.txt).
//

#include <core/environment/Singleton.h>
#include <core/logger/Logger.h>
#include <core/math/Quaternion.h>
#include <core/math/Transform.h>
#include <core/math/Vector2.h>
#include <core/math/Vector3.h>
#include <core/math/Vector4.h>
#include <core/type/TypeInfo.h>
#include <editor/core/EditorCore.h>

static const char *TAG = "EditorCore";

namespace sky::editor {

    // EditorCore is statically linked into each feature module. Only this
    // default service bundle is shared; explicit document services stay local.
    class EditorServices : public sky::Singleton<EditorServices> {
        friend class sky::Singleton<EditorServices>;

    public:
        EditorServices()
        {
            EditorCore::RegisterDefaultPropertyEditors(propertyEditors);
        }

        CommandService         commands;
        PropertyEditorRegistry propertyEditors;
        PropertyChangeNotifier propertyChanges;
    };

    CommandService &EditorCore::GetCommandService()
    {
        return EditorServices::Get()->commands;
    }

    PropertyEditorRegistry &EditorCore::GetPropertyEditors()
    {
        return EditorServices::Get()->propertyEditors;
    }

    PropertyChangeNotifier &EditorCore::GetPropertyChanges()
    {
        return EditorServices::Get()->propertyChanges;
    }

    void EditorCore::RegisterDefaultPropertyEditors(PropertyEditorRegistry &registry)
    {
        const auto registerType = [&registry](const Uuid &type, PropertyEditorKind kind) {
            if (!registry.HasType(type)) {
                registry.RegisterType(type, kind);
            }
        };
        registerType(TypeInfo<Vector2>::RegisteredId(), PropertyEditorKind::Vector);
        registerType(TypeInfo<Vector3>::RegisteredId(), PropertyEditorKind::Vector);
        registerType(TypeInfo<Vector4>::RegisteredId(), PropertyEditorKind::Vector);
        registerType(TypeInfo<Quaternion>::RegisteredId(), PropertyEditorKind::Rotation);

        const Uuid transformType  = TypeInfo<Transform>::RegisteredId();
        const auto registerMember = [&](const char *member, const char *label, int order) {
            if (registry.GetMemberAppearance(transformType, member) == nullptr) {
                registry.RegisterMemberAppearance(transformType, member, {label, order, true});
            }
        };
        registerMember("translation", "Position", 0);
        registerMember("rotation", "Rotation", 1);
        registerMember("scale", "Scale", 2);
    }

    void EditorCore::Init()
    {
        LOG_I(TAG, "EditorCore init");
    }

    void EditorCore::Shutdown()
    {
        EditorCore::GetCommandService().Clear();
        LOG_I(TAG, "EditorCore shutdown");
    }

} // namespace sky::editor

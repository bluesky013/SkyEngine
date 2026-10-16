#include <core/environment/Environment.h>
#include <core/platform/Platform.h>
#include <editor/core/EditorCore.h>
#include <editor/core/asset/EditorAssetCatalog.h>
#include <editor/core/command/UndoCommand.h>
#include <editor/ui/UiTheme.h>
#include <editor/widgets/ReflectedFormView.h>

#include <memory>

namespace {

    class ProbeCommand : public sky::editor::UndoCommand {
    public:
        ProbeCommand(int *target, int value) : target(target), previous(*target), value(value)
        {
        }

        void Do() override
        {
            *target = value;
            sky::editor::EditorCore::GetPropertyChanges().Notify();
        }

        void Undo() override
        {
            *target = previous;
            sky::editor::EditorCore::GetPropertyChanges().Notify();
        }

    private:
        int *target;
        int  previous;
        int  value;
    };

} // namespace

extern "C" SKY_EXPORT void EditorWidgetsProbeAttach(sky::Environment *environment)
{
    sky::Environment::Attach(environment);
}

extern "C" SKY_EXPORT sky::editor::CommandService *EditorWidgetsProbeCommands()
{
    return &sky::editor::EditorCore::GetCommandService();
}

extern "C" SKY_EXPORT sky::editor::PropertyEditorRegistry *EditorWidgetsProbeProperties()
{
    return &sky::editor::EditorCore::GetPropertyEditors();
}

extern "C" SKY_EXPORT sky::editor::PropertyChangeNotifier *EditorWidgetsProbeChanges()
{
    return &sky::editor::EditorCore::GetPropertyChanges();
}

extern "C" SKY_EXPORT const sky::editor::UiTheme *EditorWidgetsProbeTheme()
{
    return &sky::editor::GetDefaultUiTheme();
}

extern "C" SKY_EXPORT uint64_t EditorWidgetsProbeThemeRevision()
{
    return sky::editor::GetDefaultUiThemeRevision();
}

extern "C" SKY_EXPORT void EditorWidgetsProbeSetScale(float scale)
{
    sky::editor::SetDefaultUiTheme(sky::editor::MakeDarkTheme(scale));
}

extern "C" SKY_EXPORT void EditorWidgetsProbeRegisterType(const sky::Uuid *id)
{
    sky::editor::EditorCore::GetPropertyEditors().RegisterType(*id, sky::editor::PropertyEditorKind::Color);
}

extern "C" SKY_EXPORT bool EditorWidgetsProbeHasType(const sky::Uuid *id)
{
    return sky::editor::EditorCore::GetPropertyEditors().HasType(*id);
}

extern "C" SKY_EXPORT void EditorWidgetsProbeExecute(int *target, int value)
{
    sky::editor::EditorCore::GetCommandService().Execute(std::make_unique<ProbeCommand>(target, value));
}

extern "C" SKY_EXPORT void EditorWidgetsProbeUndo()
{
    sky::editor::EditorCore::GetCommandService().Undo();
}

extern "C" SKY_EXPORT sky::editor::IEditorAssetCatalog *EditorWidgetsProbeCatalog()
{
    return sky::editor::GetEditorAssetCatalog();
}

extern "C" SKY_EXPORT bool EditorWidgetsProbeDefaultForm()
{
    sky::editor::ReflectedFormView view(nullptr, "Probe");
    return &view.Commands() == &sky::editor::EditorCore::GetCommandService() && &view.Skin().Theme() == &sky::editor::GetDefaultUiTheme();
}

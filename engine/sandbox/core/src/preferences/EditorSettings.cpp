//
// Registration for the reflected editor settings (see EditorSettings.h).
//

#include <editor/core/preferences/EditorSettings.h>

#include <core/environment/Once.h>
#include <framework/serialization/Any.h>
#include <framework/serialization/PropertyCommon.h>
#include <framework/serialization/SerializationContext.h>

#include <string>

namespace sky::editor {

    namespace {

        // Convenience for the metadata values ReadPropertyAttributes expects (string / int / double).
        Any Label(const char *text)
        {
            return Any(std::string(text));
        }

        Any Order(int32_t order)
        {
            return Any(order);
        }

        Any Range(double min, double max, double step = 0.0)
        {
            (void)min;
            (void)max;
            (void)step;
            return Any(0.0);
        }

    } // namespace

    void RegisterEditorSettings(sky::SerializationContext *context)
    {
        // Cross-DLL one-shot guard (EditorCore is a static lib linked into each module DLL).
        if (context == nullptr || !sky::Once::Get()->Run("EditorSettings")) {
            return;
        }

        context->Register<EditorLanguage>("EditorLanguage").Enum(EditorLanguage::English, "English").Enum(EditorLanguage::Chinese, "Chinese");
        context->Register<EditorTheme>("EditorTheme").Enum(EditorTheme::Dark, "Dark").Enum(EditorTheme::Light, "Light");

        context->Register<GeneralSettings>("GeneralSettings")
            .Member<&GeneralSettings::language>("language")
            .Property(CommonPropertyKey::LABEL, Label("Language"))
            .Property(CommonPropertyKey::ORDER, Order(0))
            .Member<&GeneralSettings::theme>("theme")
            .Property(CommonPropertyKey::LABEL, Label("Theme"))
            .Property(CommonPropertyKey::ORDER, Order(1))
            .Member<&GeneralSettings::autosave>("autosave")
            .Property(CommonPropertyKey::LABEL, Label("Autosave"))
            .Property(CommonPropertyKey::ORDER, Order(2));

        context->Register<EditorViewSettings>("EditorViewSettings")
            .Member<&EditorViewSettings::uiScale>("uiScale")
            .Property(CommonPropertyKey::LABEL, Label("UI Scale"))
            .Property(CommonPropertyKey::RANGE_MIN, Any(0.5))
            .Property(CommonPropertyKey::RANGE_MAX, Any(2.0))
            .Property(CommonPropertyKey::RANGE_STEP, Any(0.05))
            .Property(CommonPropertyKey::ORDER, Order(0))
            .Member<&EditorViewSettings::gridSize>("gridSize")
            .Property(CommonPropertyKey::LABEL, Label("Grid Size"))
            .Property(CommonPropertyKey::RANGE_MIN, Any(1.0))
            .Property(CommonPropertyKey::RANGE_MAX, Any(100.0))
            .Property(CommonPropertyKey::ORDER, Order(1))
            .Member<&EditorViewSettings::snap>("snap")
            .Property(CommonPropertyKey::LABEL, Label("Snap"))
            .Property(CommonPropertyKey::ORDER, Order(2))
            .Member<&EditorViewSettings::undoDepth>("undoDepth")
            .Property(CommonPropertyKey::LABEL, Label("Undo Depth"))
            .Property(CommonPropertyKey::RANGE_MIN, Any(1.0))
            .Property(CommonPropertyKey::RANGE_MAX, Any(256.0))
            .Property(CommonPropertyKey::ORDER, Order(3));

        context->Register<RenderingSettings>("RenderingSettings")
            .Member<&RenderingSettings::vsync>("vsync")
            .Property(CommonPropertyKey::LABEL, Label("V-Sync"))
            .Property(CommonPropertyKey::ORDER, Order(0))
            .Member<&RenderingSettings::resolutionScale>("resolutionScale")
            .Property(CommonPropertyKey::LABEL, Label("Resolution Scale"))
            .Property(CommonPropertyKey::RANGE_MIN, Any(0.5))
            .Property(CommonPropertyKey::RANGE_MAX, Any(2.0))
            .Property(CommonPropertyKey::RANGE_STEP, Any(0.05))
            .Property(CommonPropertyKey::ORDER, Order(1));
    }

} // namespace sky::editor

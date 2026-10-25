//
// Reflected editor settings: one plain struct per Preferences page (see
// docs/editor/preferences-and-settings.md). The Preferences dialog renders/edits these through
// ReflectedFormView, driven by reflection metadata, so no per-type control is hardcoded. The
// copy-constructible structs are edited through owned session values and committed on OK/Apply.
//

#pragma once

#include <cstdint>

namespace sky {
    class SerializationContext;
} // namespace sky

namespace sky::editor {

    enum class EditorLanguage : int32_t {
        English = 0,
        Chinese = 1,
    };

    enum class EditorTheme : int32_t {
        Dark  = 0,
        Light = 1,
    };

    struct GeneralSettings {
        EditorLanguage language = EditorLanguage::English;
        EditorTheme    theme    = EditorTheme::Dark;
        bool           autosave = true;
    };

    struct EditorViewSettings {
        float   uiScale   = 1.0f;
        float   gridSize  = 10.0f;
        bool    snap      = true;
        int32_t undoDepth = 32;
    };

    struct RenderingSettings {
        bool  vsync           = true;
        float resolutionScale = 1.0f;
    };

    // Registers the settings types + editor metadata (label/range/enum) with the reflection
    // context. Owned by the SandboxModule metadata phase.
    void RegisterEditorSettings(sky::SerializationContext *context);

} // namespace sky::editor

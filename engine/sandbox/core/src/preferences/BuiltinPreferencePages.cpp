#include <editor/core/preferences/BuiltinPreferencePages.h>

#include <framework/serialization/SerializationContext.h>

#include <algorithm>
#include <cmath>

namespace sky::editor {
    namespace {
        PreferenceResult Invalid(const char *page, const char *field, const char *message)
        {
            return {PreferenceCode::ValidationFailed, page, field, message};
        }
    } // namespace

    BuiltinPreferencePages::BuiltinPreferencePages(PreferencePageRegistry &value) : registry(value)
    {
    }
    BuiltinPreferencePages::~BuiltinPreferencePages()
    {
        Reset();
    }

    void BuiltinPreferencePages::Reset()
    {
        for (auto registration : registrations) {
            registry.RemoveRegistration(registration);
        }
        registrations.clear();
    }

    PreferenceResult BuiltinPreferencePages::Bind(GeneralSettings                  &general,
                                                  EditorViewSettings               &editor,
                                                  RenderingSettings                &rendering,
                                                  std::function<PreferenceResult()> applyEditor)
    {
        // Check collisions before replacing this binding or adding any page.
        for (const auto &page : registry.GetPages()) {
            const bool owned = std::find(registrations.begin(), registrations.end(), page.registration) != registrations.end();
            if (!owned && (page.id == "general" || page.id == "editor" || page.id == "rendering")) {
                return {PreferenceCode::Conflict, page.id, "", "A built-in preference page id is already registered."};
            }
        }
        RegisterEditorSettings(SerializationContext::Get());
        Reset();
        const auto add = [this](PreferencePage page) {
            registry.Add(std::move(page));
            registrations.push_back(registry.GetPages().back().registration);
        };
        PreferencePage generalPage{"general", "General (stored only)", &general, GetTypeNode(TypeInfo<GeneralSettings>::RegisteredId())};
        generalPage.validate = [](const PreferencePage &candidate) {
            const auto &value = *static_cast<const GeneralSettings *>(candidate.object);
            if (value.language != EditorLanguage::English && value.language != EditorLanguage::Chinese)
                return Invalid("general", "language", "Unknown editor language.");
            if (value.theme != EditorTheme::Dark && value.theme != EditorTheme::Light)
                return Invalid("general", "theme", "Unknown editor theme.");
            return PreferenceResult{};
        };
        add(std::move(generalPage));
        PreferencePage editorPage{"editor", "Editor", &editor, GetTypeNode(TypeInfo<EditorViewSettings>::RegisteredId())};
        editorPage.validate = [](const PreferencePage &candidate) {
            const auto &value = *static_cast<const EditorViewSettings *>(candidate.object);
            if (!std::isfinite(value.uiScale) || value.uiScale < 0.5f || value.uiScale > 2.0f)
                return Invalid("editor", "uiScale", "UI Scale must be between 0.5 and 2.");
            if (!std::isfinite(value.gridSize) || value.gridSize < 1.0f || value.gridSize > 100.0f)
                return Invalid("editor", "gridSize", "Grid Size must be between 1 and 100.");
            if (value.undoDepth < 1 || value.undoDepth > 256)
                return Invalid("editor", "undoDepth", "Undo Depth must be between 1 and 256.");
            return PreferenceResult{};
        };
        if (applyEditor) {
            editorPage.apply = [applyEditor = std::move(applyEditor)](const PreferencePage &) { return applyEditor(); };
        }
        add(std::move(editorPage));
        PreferencePage renderingPage{"rendering", "Rendering (stored only)", &rendering, GetTypeNode(TypeInfo<RenderingSettings>::RegisteredId())};
        renderingPage.validate = [](const PreferencePage &candidate) {
            const auto &value = *static_cast<const RenderingSettings *>(candidate.object);
            if (!std::isfinite(value.resolutionScale) || value.resolutionScale < 0.5f || value.resolutionScale > 2.0f)
                return Invalid("rendering", "resolutionScale", "Resolution Scale must be between 0.5 and 2.");
            return PreferenceResult{};
        };
        add(std::move(renderingPage));
        return {};
    }

    float ComputeEditorUiScale(float deviceScale, float userScale)
    {
        deviceScale = std::isfinite(deviceScale) ? std::clamp(deviceScale, 0.5f, 4.0f) : 1.0f;
        userScale   = std::isfinite(userScale) ? std::clamp(userScale, 0.5f, 2.0f) : 1.0f;
        return std::clamp(deviceScale * userScale, 0.5f, 4.0f);
    }
} // namespace sky::editor

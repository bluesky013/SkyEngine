#pragma once

#include <editor/core/preferences/EditorSettings.h>
#include <editor/core/preferences/PreferencePageRegistry.h>

#include <functional>

namespace sky::editor {

    // Owns only the host's three borrowed settings registrations. The registry,
    // settings and application callback must outlive this binding.
    class BuiltinPreferencePages {
    public:
        explicit BuiltinPreferencePages(PreferencePageRegistry &registry);
        ~BuiltinPreferencePages();
        BuiltinPreferencePages(const BuiltinPreferencePages &)            = delete;
        BuiltinPreferencePages &operator=(const BuiltinPreferencePages &) = delete;

        PreferenceResult
        Bind(GeneralSettings &general, EditorViewSettings &editor, RenderingSettings &rendering, std::function<PreferenceResult()> applyEditor = {});
        void Reset();

    private:
        PreferencePageRegistry &registry;
        std::vector<uint64_t>   registrations;
    };

    // Device DPI and the user preference are independent inputs. The product
    // is the physical theme scale used by every main/floating editor window.
    float ComputeEditorUiScale(float deviceScale, float userScale);

} // namespace sky::editor

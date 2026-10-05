//
// Created on 2026/10/05.
//

#include <editor/shell/ReflectedConfigPanel.h>

#include <ui/text/UITextSystem.h>

#include <utility>

namespace sky::editor {

    ReflectedConfigPanel::ReflectedConfigPanel(sky::ui::UITextSystem *text, IEditorConfigSource *source, std::string title)
        : ReflectedFormView(text, std::move(title))
        , configSource(source)
    {
        ResolveConfigs();
    }

    void ReflectedConfigPanel::OnViewTick()
    {
        ResolveConfigs();
    }

    void ReflectedConfigPanel::ResolveConfigs()
    {
        if (configSource == nullptr) {
            Bind(PropertyObject{});
            return;
        }
        const std::vector<NamedPropertyObject> configs = configSource->GetConfigs();
        Bind(configs.empty() ? PropertyObject{} : configs.front().object);
    }

} // namespace sky::editor

//
// IconRegistry: process-wide (cross-DLL) name -> UI texture id map, so any editor widget/dialog can
// resolve an icon by name (toolbar icon-only buttons, menus, ...). Populated by the shell when it
// rasterizes resources/icons/*.svg. See docs/editor/ui-standards.md.
//

#pragma once

#include <core/environment/Singleton.h>

#include <cstdint>
#include <string>
#include <unordered_map>

namespace sky::editor {

    class IconRegistry : public sky::Singleton<IconRegistry> {
        friend class sky::Singleton<IconRegistry>;

    public:
        void Set(const std::string &name, uint32_t texture)
        {
            icons[name] = texture;
        }
        // Returns 0 (invalid) when the icon is unknown.
        uint32_t Find(const std::string &name) const
        {
            const auto it = icons.find(name);
            return it != icons.end() ? it->second : 0;
        }
        bool Has(const std::string &name) const
        {
            return icons.find(name) != icons.end();
        }

    private:
        std::unordered_map<std::string, uint32_t> icons;
    };

} // namespace sky::editor

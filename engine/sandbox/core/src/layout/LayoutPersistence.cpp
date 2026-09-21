//
// Created on 2026/09/21.
//

#include <editor/core/layout/LayoutPersistence.h>
#include <framework/platform/PlatformBase.h>
#include <fstream>
#include <sstream>

namespace sky::editor {

    bool LayoutPersistence::Save(const LayoutModel &model, const std::string &path)
    {
        if (path.empty()) {
            return false;
        }
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        if (!file.is_open()) {
            return false;
        }
        const std::string json = model.ToJson(2);
        file.write(json.data(), static_cast<std::streamsize>(json.size()));
        return file.good();
    }

    bool LayoutPersistence::Load(const std::string &path, LayoutModel &model, const PanelRegistry *registry,
                                 std::vector<std::string> *warnings)
    {
        if (path.empty()) {
            return false;
        }
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) {
            return false;
        }
        std::ostringstream buffer;
        buffer << file.rdbuf();
        return LayoutModel::FromJson(buffer.str(), model, registry, warnings);
    }

    std::string LayoutPersistence::GetDefaultPath()
    {
        const std::string dir = Platform::Get()->GetUserConfigPath();
        if (dir.empty()) {
            return {};
        }
        return dir + "editor_layout.json";
    }

} // namespace sky::editor

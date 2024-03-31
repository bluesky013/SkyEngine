//
// Created by blues on 2024/3/30.
//

#include <editor/widgets/DirectoryBrowser.h>
#include <filesystem>

namespace sky::editor {

    static void Traverse(const std::filesystem::path &path)
    {
        bool isDir = std::filesystem::is_directory(path);

        if (isDir) {
            if (ImGui::TreeNode(path.filename().string().c_str())) {
                for (const auto &entry: std::filesystem::directory_iterator(path)) {
                    Traverse(entry);
                }
                ImGui::TreePop();
            }
        } else {
            if (ImGui::TreeNodeEx(path.filename().string().c_str(), ImGuiTreeNodeFlags_Leaf)) {
                ImGui::TreePop();
            }
        }

    }

    void DirectoryBrowser::BindEvent(EventID id)
    {
        binder.Bind(this, id);
    }

    void DirectoryBrowser::OnClicked()
    {
        isOpen = true;
    }

    void DirectoryBrowser::Execute(ImContext &context)
    {
        if (!isOpen) {
            return;
        }

        if (ImGui::Begin(name.c_str(), &isOpen)) {
            for (auto &path : rootPaths) {
                Traverse(path);
            }
        }
        ImGui::End();
    }

} // namespace sky::editor
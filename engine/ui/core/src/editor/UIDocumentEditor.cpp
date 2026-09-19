//
// Created on 2026/09/19.
//

#include <ui/editor/UIDocumentEditor.h>
#include <ui/data/UIDocument.h>

#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

namespace sky::ui {

    namespace {

        void WriteNode(rapidjson::Writer<rapidjson::StringBuffer> &writer, const UIElement *element)
        {
            writer.StartObject();
            writer.Key("type");
            writer.String(element->GetTypeName());

            if (!element->GetName().empty()) {
                writer.Key("name");
                writer.String(element->GetName().c_str());
            }
            if (!element->IsVisible()) {
                writer.Key("visible");
                writer.Bool(false);
            }
            if (!element->IsEnabled()) {
                writer.Key("enabled");
                writer.Bool(false);
            }

            const auto &classes = element->GetStyleClasses();
            if (!classes.empty()) {
                writer.Key("style");
                writer.StartArray();
                for (const auto &className : classes) {
                    writer.String(className.c_str());
                }
                writer.EndArray();
            }

            const auto &children = element->GetChildren();
            if (!children.empty()) {
                writer.Key("children");
                writer.StartArray();
                for (const auto &child : children) {
                    WriteNode(writer, child.get());
                }
                writer.EndArray();
            }
            writer.EndObject();
        }

        UIElement *FindRecursive(UIElement *element, const std::string &name)
        {
            if (element == nullptr) {
                return nullptr;
            }
            if (element->GetName() == name) {
                return element;
            }
            for (const auto &child : element->GetChildren()) {
                if (UIElement *found = FindRecursive(child.get(), name)) {
                    return found;
                }
            }
            return nullptr;
        }

    } // namespace

    UIDocumentEditor::UIDocumentEditor()
        : registry(UIElementRegistry::CreateDefault())
    {
        root = registry.Create("Panel");
        if (root != nullptr) {
            root->SetName("Root");
        }
    }

    UIElement *UIDocumentEditor::Find(const std::string &name) const
    {
        return FindRecursive(root.get(), name);
    }

    void UIDocumentEditor::Snapshot()
    {
        undoStack.push_back(Serialize());
        redoStack.clear();
    }

    UIElement *UIDocumentEditor::AddChild(const std::string &parentName, const std::string &type)
    {
        UIElement *parent = parentName.empty() ? root.get() : Find(parentName);
        UIElementPtr child = registry.Create(type);
        if (parent == nullptr || child == nullptr) {
            return nullptr;
        }

        Snapshot();
        return parent->AddChild(std::move(child));
    }

    bool UIDocumentEditor::Remove(const std::string &name)
    {
        UIElement *element = Find(name);
        if (element == nullptr || element->GetParent() == nullptr) {
            return false;
        }

        Snapshot();
        element->GetParent()->RemoveChild(element);
        return true;
    }

    bool UIDocumentEditor::Rename(const std::string &name, const std::string &newName)
    {
        UIElement *element = Find(name);
        if (element == nullptr) {
            return false;
        }

        Snapshot();
        element->SetName(newName);
        return true;
    }

    bool UIDocumentEditor::MoveUp(const std::string &name)
    {
        UIElement *element = Find(name);
        if (element == nullptr || element->GetParent() == nullptr) {
            return false;
        }
        Snapshot();
        if (!element->GetParent()->MoveChild(element, -1)) {
            undoStack.pop_back();
            return false;
        }
        return true;
    }

    bool UIDocumentEditor::MoveDown(const std::string &name)
    {
        UIElement *element = Find(name);
        if (element == nullptr || element->GetParent() == nullptr) {
            return false;
        }
        Snapshot();
        if (!element->GetParent()->MoveChild(element, 1)) {
            undoStack.pop_back();
            return false;
        }
        return true;
    }

    std::string UIDocumentEditor::Serialize() const
    {
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);

        writer.StartObject();
        writer.Key("root");
        WriteNode(writer, root.get());
        writer.EndObject();
        return buffer.GetString();
    }

    bool UIDocumentEditor::Deserialize(const std::string &json)
    {
        auto document = UIDocumentLoader::LoadFromString(json, registry, nullptr);
        if (document == nullptr || document->root == nullptr) {
            return false;
        }
        root = std::move(document->root);
        return true;
    }

    bool UIDocumentEditor::Undo()
    {
        if (undoStack.empty()) {
            return false;
        }
        redoStack.push_back(Serialize());
        const std::string state = undoStack.back();
        undoStack.pop_back();
        return Deserialize(state);
    }

    bool UIDocumentEditor::Redo()
    {
        if (redoStack.empty()) {
            return false;
        }
        undoStack.push_back(Serialize());
        const std::string state = redoStack.back();
        redoStack.pop_back();
        return Deserialize(state);
    }

} // namespace sky::ui

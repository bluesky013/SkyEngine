//
// Created on 2026/09/19.
//

#pragma once

#include <ui/UIElement.h>

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace sky::ui {

    class IUIDataProvider;
    class IUIAssetResolver;
    class UIElementRegistry;

    enum class UIDiagnosticSeverity : uint8_t {
        WARNING = 0,
        ERROR,
    };

    struct UIDiagnostic {
        UIDiagnosticSeverity severity = UIDiagnosticSeverity::WARNING;
        std::string nodePath;
        std::string code;
        std::string message;
    };

    struct UIBinding {
        UIElement *target = nullptr;
        std::string targetPath;
        std::string sourcePath;
        std::string converter;
        float remapIn0 = 0.0f;
        float remapIn1 = 1.0f;
        float remapOut0 = 0.0f;
        float remapOut1 = 0.0f;

        bool Apply(const IUIDataProvider &provider) const;
    };

    class UIDocument {
    public:
        UIElementPtr root;
        std::vector<UIBinding> bindings;
        std::vector<UIDiagnostic> diagnostics;

        UIElement *FindByName(const std::string &name) const;
        bool HasErrors() const;

        // Re-applies bindings when the provider version changed; returns whether it applied.
        bool ApplyBindings(const IUIDataProvider &provider);

        void Index();

    private:
        void IndexInto(UIElement *element);

        std::unordered_map<std::string, UIElement *> nameIndex;
        uint32_t lastVersion = 0;
        bool hasVersion = false;
    };

    class UIDocumentLoader {
    public:
        static std::unique_ptr<UIDocument> LoadFromString(const std::string &json,
                                                          const UIElementRegistry &registry,
                                                          IUIAssetResolver *resolver);
    };

} // namespace sky::ui

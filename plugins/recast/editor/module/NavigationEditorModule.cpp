//
// Created on 2026/10/30.
//

#include <recast/editor/NavigationEditorService.h>

#include <core/logger/Logger.h>
#include <editor/core/extension/EditorExtensionRegistry.h>
#include <framework/application/ModuleManager.h>
#include <framework/interface/IModule.h>

namespace sky::ai {
    class NavigationEditorExtension final : public editor::EditorExtension {
    public:
        NavigationEditorExtension(std::shared_ptr<NavigationEditorService> value, std::shared_ptr<const void> code)
            : lease(std::move(code)), service(std::move(value))
        {
        }
        const char *GetName() const override
        {
            return "NavigationEditor";
        }
        void Register() override
        {
            if (!service->IsActive())
                return;
            if (!service->Register(lease))
                LOG_E("NavigationEditor", "Editor action/overlay registration failed");
        }
        void Unregister() override
        {
            service->Unregister();
        }

    private:
        std::shared_ptr<const void>              lease;
        std::shared_ptr<NavigationEditorService> service;
    };

    class NavigationEditorModule final : public IModule {
    public:
        bool Init(const StartArguments &) override
        {
            auto *manager = ModuleManager::Current();
            auto  lease   = manager ? manager->RetainModule("RecastNavigation.Editor") : nullptr;
            if (!lease)
                return false;
            service        = std::make_shared<NavigationEditorService>();
            auto extension = std::make_unique<NavigationEditorExtension>(service, lease);
            editor::EditorExtensionRegistry::Get()->Add(std::move(extension), std::move(lease));
            return true;
        }
        void Tick(float) override
        {
            if (service)
                service->Tick();
        }
        void Shutdown() override
        {
            if (service)
                service->Deactivate();
            service.reset();
        }

    private:
        std::shared_ptr<NavigationEditorService> service;
    };
} // namespace sky::ai
REGISTER_MODULE_TAGGED(sky::ai::NavigationEditorModule)

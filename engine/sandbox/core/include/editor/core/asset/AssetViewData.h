//
// Registered per-asset-type reflected view data (see asset-viewer-widget, D5.1).
// The owning module's editor registers a provider that turns an asset into a reflected
// "view struct" (an owning Any + its TypeNode); the viewer builds a ReflectedFormView for
// the DATA section. UI-free (EditorCore) so providers are toolkit-independent.
//

#pragma once

#include <core/environment/Singleton.h>
#include <core/util/Uuid.h>
#include <framework/serialization/Any.h>
#include <framework/serialization/SerializationFactory.h>

#include <string>
#include <unordered_map>

namespace sky::editor {

    // An owning reflected view value plus its type; invalid when nothing could be built.
    struct AssetViewData {
        sky::Any             object;
        const sky::TypeNode *type = nullptr;

        bool IsValid() const
        {
            return type != nullptr && object.Data() != nullptr;
        }
    };

    // Produces reflected view data for one asset type. Implemented by the owning module's
    // editor (aurora / content) and registered with AssetViewDataRegistry.
    class IAssetViewDataProvider {
    public:
        virtual ~IAssetViewDataProvider() = default;

        // Loads/peeks the asset and fills a view struct; invalid when unavailable.
        virtual AssetViewData Build(const Uuid &uuid, const std::string &type) = 0;
    };

    // Process-wide (cross-DLL) registry keyed by asset type.
    class AssetViewDataRegistry : public sky::Singleton<AssetViewDataRegistry> {
        friend class sky::Singleton<AssetViewDataRegistry>;

    public:
        void Register(const std::string &type, IAssetViewDataProvider *provider)
        {
            providers[type] = provider;
        }
        void Unregister(const std::string &type)
        {
            providers.erase(type);
        }
        bool Has(const std::string &type) const
        {
            return providers.find(type) != providers.end();
        }
        AssetViewData Build(const std::string &type, const Uuid &uuid) const
        {
            const auto it = providers.find(type);
            return it != providers.end() ? it->second->Build(uuid, type) : AssetViewData{};
        }

    private:
        std::unordered_map<std::string, IAssetViewDataProvider *> providers;
    };

} // namespace sky::editor

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

#include <memory>
#include <string>
#include <utility>
#include <unordered_map>

namespace sky::editor {

    // An owning reflected view value plus its type; invalid when nothing could be built.
    struct AssetViewData {
        AssetViewData() = default;
        AssetViewData(sky::Any value, const sky::TypeNode *node) : object(std::move(value)), type(node) {}
        AssetViewData(const AssetViewData &) = default;
        AssetViewData(AssetViewData &&) noexcept = default;
        AssetViewData &operator=(AssetViewData value) noexcept
        {
            moduleLease.swap(value.moduleLease);
            std::swap(object, value.object);
            std::swap(type, value.type);
            return *this;
        }
        std::shared_ptr<const void> moduleLease;
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
        void Register(const std::string &type, IAssetViewDataProvider *provider, std::shared_ptr<const void> lease = {})
        {
            providers[type] = {std::move(lease), provider};
        }
        void Unregister(const std::string &type, IAssetViewDataProvider *expected = nullptr)
        {
            const auto found = providers.find(type);
            if (found != providers.end() && (!expected || expected == found->second.provider)) providers.erase(found);
        }
        bool Has(const std::string &type) const
        {
            return providers.find(type) != providers.end();
        }
        AssetViewData Build(const std::string &type, const Uuid &uuid) const
        {
            const auto it = providers.find(type);
            if (it == providers.end()) return {};
            const auto entry = it->second;
            auto result = entry.provider->Build(uuid, type);
            result.moduleLease = entry.moduleLease;
            return result;
        }

    private:
        struct Entry {
            std::shared_ptr<const void> moduleLease;
            IAssetViewDataProvider *provider = nullptr;
        };
        std::unordered_map<std::string, Entry> providers;
    };

} // namespace sky::editor

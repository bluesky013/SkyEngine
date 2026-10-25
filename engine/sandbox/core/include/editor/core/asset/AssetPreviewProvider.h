//
// Preview scenes share the editor renderer; callers retain a session until CloseSession.
//

#pragma once

#include <core/environment/Singleton.h>
#include <core/util/Uuid.h>
#include <core/shapes/AABB.h>
#include <editor/core/viewport/EditorCamera.h>
#include <framework/world/World.h>

#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace sky::editor {

    enum class AssetPreviewErrorCode { Unavailable, InvalidArgument, StaleSession, Capacity };
    struct AssetPreviewError {
        AssetPreviewErrorCode code;
        std::string           message;
    };
    struct AssetPreviewHandle {
        Uuid session, generation;
        bool operator==(const AssetPreviewHandle &) const = default;
    };
    struct AssetPreviewScene {
        // The World and callbacks must be destroyed before their defining code is released.
        std::shared_ptr<const void> moduleLease;
        WorldPtr                    world;
        EditorCamera                camera;
        uint32_t                    width = 320, height = 240;
    };
    enum class AssetPreviewState { Pending, Ready, Closing, Closed, Failed };
    struct AssetPreviewStatus {
        AssetPreviewState state    = AssetPreviewState::Pending;
        uint32_t          texture  = 0;
        uint64_t          revision = 0;
        std::string       diagnostic;
        std::vector<AABB> bounds;
    };
    using AssetPreviewResult       = std::variant<AssetPreviewError, AssetPreviewHandle>;
    using AssetPreviewStatusResult = std::variant<AssetPreviewError, AssetPreviewStatus>;

    // Optional provider of asset previews, registered by an extension.
    class IAssetPreviewProvider {
    public:
        virtual ~IAssetPreviewProvider() = default;

        // Returns true when the provider can host/render a preview for the asset (uuid + type).
        virtual bool GetPreview(const Uuid &, const std::string &)
        {
            return false;
        }
        virtual AssetPreviewResult CreateSession(AssetPreviewScene)
        {
            return AssetPreviewError{AssetPreviewErrorCode::Unavailable, "Preview sessions unavailable"};
        }
        virtual AssetPreviewStatusResult Update(AssetPreviewHandle, EditorCamera)
        {
            return AssetPreviewError{AssetPreviewErrorCode::Unavailable, "Preview sessions unavailable"};
        }
        virtual AssetPreviewStatusResult Resize(AssetPreviewHandle, uint32_t, uint32_t)
        {
            return AssetPreviewError{AssetPreviewErrorCode::Unavailable, "Preview sessions unavailable"};
        }
        virtual AssetPreviewStatusResult CloseSession(AssetPreviewHandle)
        {
            return AssetPreviewError{AssetPreviewErrorCode::Unavailable, "Preview sessions unavailable"};
        }
        virtual AssetPreviewStatusResult Status(AssetPreviewHandle) const
        {
            return AssetPreviewError{AssetPreviewErrorCode::Unavailable, "Preview sessions unavailable"};
        }
    };

    // Process-wide (cross-DLL) holder for the active preview provider.
    class AssetPreviewProviderRegistry : public sky::Singleton<AssetPreviewProviderRegistry> {
        friend class sky::Singleton<AssetPreviewProviderRegistry>;

    public:
        void SetProvider(IAssetPreviewProvider *value)
        {
            provider = value;
        }
        IAssetPreviewProvider *GetProvider() const
        {
            return provider;
        }

    private:
        IAssetPreviewProvider *provider = nullptr;
    };

} // namespace sky::editor

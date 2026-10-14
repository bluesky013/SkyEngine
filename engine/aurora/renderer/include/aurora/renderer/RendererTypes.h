#pragma once

#include <aurora/rhi/Core.h>
#include <core/math/Matrix4.h>
#include <core/name/Name.h>
#include <cstdint>
#include <limits>
#include <string>
#include <variant>

namespace sky::aurora {

    // Host-assigned IDs: reuse only after destroy acknowledgement with a new generation; retire on generation exhaustion.
    template <typename Tag>
    struct RenderHandle {
        uint32_t index      = std::numeric_limits<uint32_t>::max();
        uint32_t generation = 0;

        // Representation check only; does not establish registry membership.
        constexpr bool IsValid() const noexcept
        {
            return index != std::numeric_limits<uint32_t>::max() && generation != 0;
        }
        bool operator==(const RenderHandle &) const = default;
    };

    using SceneId        = RenderHandle<struct SceneHandleTag>;
    using RenderEntityId = RenderHandle<struct RenderEntityHandleTag>;
    using ViewId         = RenderHandle<struct ViewHandleTag>;
    using ViewportId     = RenderHandle<struct ViewportHandleTag>;

    struct RenderEntityRef {
        SceneId        scene;
        RenderEntityId entity;
        bool           operator==(const RenderEntityRef &) const = default;
    };

    enum class RendererErrorCode : uint8_t {
        NOT_READY,
        INVALID_ARGUMENT,
        INVALID_PACKET,
        STALE_HANDLE,
        OUT_OF_MEMORY,
        DEVICE_LOST,
        UNSUPPORTED,
        INTERNAL_ERROR
    };

    struct RendererError {
        RendererErrorCode code = RendererErrorCode::NOT_READY;
        std::string       message;
    };

    enum class RendererState : uint8_t { STARTING, RUNNING, STOPPING, STOPPED, FAILED };
    enum class PacketSubmitStatus : uint8_t { ACCEPTED, BUSY, REJECTED };

    struct PacketSubmitResult {
        PacketSubmitStatus status = PacketSubmitStatus::REJECTED;
        RendererError      error; // meaningful only for REJECTED
    };

    struct RendererInitInfo {
        API      backend                 = API::DEFAULT;
        uint32_t inflightFrames          = 2;
        uint32_t maxPendingPackets       = 2;
        uint32_t prepareWorkers          = 1;
        uint32_t encodeWorkers           = 1;
        uint64_t maxPendingResourceBytes = 256ull * 1024 * 1024;
        uint64_t maxUploadBytesPerFrame  = 16ull * 1024 * 1024;
    };

    struct RendererInfo {
        API      backend     = API::DEFAULT; // resolved backend after successful Create
        uint64_t deviceEpoch = 0;            // nonzero, stable for this renderer's lifetime
    };

    struct SceneDesc {
        Name name;
    };

    inline Matrix4 RenderIdentityMatrix()
    {
        return Matrix4{{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}};
    }

    struct ViewDesc {
        SceneId    scene;
        ViewportId output;
        Matrix4    view       = RenderIdentityMatrix();
        Matrix4    projection = RenderIdentityMatrix();
        Vector3    cameraPosition;
        uint64_t   visibilityMask = ~uint64_t{0};
    };

    enum class ViewportKind : uint8_t { OFFSCREEN, NATIVE_WINDOW };

    struct ViewportDesc {
        ViewportKind kind = ViewportKind::OFFSCREEN;
        Extent2D     extent{1, 1};
        PixelFormat  format = PixelFormat::RGBA8_UNORM;
        // Borrowed native window; host retains it until ViewportDestroyed acknowledgement.
        uintptr_t nativeWindow = 0;
    };

    // CPU commands applied; does not imply GPU/presentation retirement.
    struct PacketApplied {
        uint64_t sequence = 0;
    };
    // Retires all scene entities; referencing Views stop scene rendering but retain their lifetime/output.
    struct SceneDestroyed {
        SceneId scene;
    };
    struct EntityDestroyed {
        RenderEntityRef entity;
    };
    struct ViewDestroyed {
        ViewId view;
    };
    struct ViewportDestroyed {
        ViewportId viewport;
    };
    struct RendererStopped {};
    struct RendererFailure {
        uint64_t      packetSequence = 0;
        RendererError error;
    };

} // namespace sky::aurora

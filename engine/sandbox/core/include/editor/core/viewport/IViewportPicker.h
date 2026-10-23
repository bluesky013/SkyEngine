//
// Created on 2026/10/23.
//

#pragma once

#include <array>
#include <core/math/Vector3.h>
#include <core/shapes/AABB.h>
#include <core/util/Uuid.h>
#include <cstdint>
#include <editor/core/viewport/EditorCamera.h>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace sky::editor {
    enum class ViewportPickStatus { HIT, MISS, PENDING, UNSUPPORTED, ERROR };
    enum class ViewportPickPolicy { EDITOR_DEFAULT, GEOMETRY_ONLY, VISIBLE_SURFACE };
    enum class ViewportPickSemantics { GEOMETRY_SURFACE, EDITOR_PROXY, VISIBLE_SURFACE };
    enum class ViewportPickReason { NONE, OCCLUDED, RESOURCE_LIMIT, INVALID_REQUEST, STALE_REQUEST, UNSUPPORTED_COVERAGE };

    struct ViewportPickContext {
        uint64_t document = 0, scene = 0, camera = 0, layout = 0, surface = 0, request = 0;
        uint64_t policy = 0, providers = 0;
        bool     operator==(const ViewportPickContext &) const = default;
    };

    struct ViewportPickView {
        EditorCamera camera;
        Vector2      point{}, extent{};
        uint32_t     imageWidth = 0, imageHeight = 0, pixelX = 0, pixelY = 0;
        bool         available = false;
    };
    struct ViewportPickTarget {
        Uuid     actor, component;
        uint64_t instance = 0, generation = 0, provider = 0;
        uint32_t part = 0, subelement = 0;
        bool     operator==(const ViewportPickTarget &) const = default;
    };
    struct ViewportPickRequest {
        ViewportPickContext context;
        Vector3             origin, direction;
        float               nearDistance = 0, farDistance = 10000;
        ViewportPickView    view;
        ViewportPickPolicy  policy         = ViewportPickPolicy::EDITOR_DEFAULT;
        uint64_t            visibilityMask = ~uint64_t{0};
        std::vector<Uuid>   include, ignore, locked;
    };

    struct ViewportPickResult {
        ViewportPickContext    context;
        ViewportPickStatus     status = ViewportPickStatus::PENDING;
        Uuid                   actor;
        uint32_t               part     = 0;
        float                  distance = 0;
        std::optional<Vector3> position, normal;
        std::string            message;
        ViewportPickTarget     target;
        ViewportPickSemantics  semantics = ViewportPickSemantics::GEOMETRY_SURFACE;
        ViewportPickReason     reason    = ViewportPickReason::NONE;
    };

    enum class ViewportProxyDepth { TESTED, OVERLAY };
    struct ViewportTriangleProxy {
        Vector3 a, b, c;
    };
    struct ViewportSegmentProxy {
        Vector3 a, b;
        float   width = 1, tolerance = 0;
    };
    struct ViewportBillboardProxy {
        Vector3 center;
        Vector2 size{16, 16};
    };
    struct ViewportPickProxy {
        ViewportPickTarget                                                                target;
        std::variant<ViewportTriangleProxy, ViewportSegmentProxy, ViewportBillboardProxy> shape;
        ViewportProxyDepth                                                                depth          = ViewportProxyDepth::TESTED;
        uint64_t                                                                          visibilityMask = ~uint64_t{0};
        int32_t                                                                           priority       = 0;
        uint32_t                                                                          color          = 0xffffffff;
        bool                                                                              selectable = true, occluder = true;
    };
    struct ViewportPickRay {
        Vector3 origin, direction;
        float   nearDistance = 0, farDistance = 0;
    };
    struct ViewportPickGeometryBudget {
        uint32_t work         = 0;
        double   milliseconds = 0;
    };
    struct ViewportPickGeometryResult {
        ViewportPickStatus     status   = ViewportPickStatus::PENDING;
        float                  distance = 0;
        std::optional<Vector3> position, normal;
        uint64_t               instance = 0;
        uint32_t               part = 0, subelement = 0;
        ViewportPickReason     reason = ViewportPickReason::NONE;
        std::string            message;
    };
    class IViewportPickGeometryCursor {
    public:
        // Destruction cancels continuation; it must not wait for background work.
        virtual ~IViewportPickGeometryCursor() = default;
    };
    class IViewportPickGeometry {
    public:
        virtual ~IViewportPickGeometry() = default;
        // Host-thread, immutable world geometry. Return the nearest surface or an explicit incomplete state.
        // Consume work/time budget; Pending retains cursor. No loading, building, blocking or selection writes.
        virtual ViewportPickGeometryResult
        Raycast(const ViewportPickRay &ray, ViewportPickGeometryBudget &budget, std::unique_ptr<IViewportPickGeometryCursor> &cursor) const = 0;
    };
    struct ViewportPickGeometry {
        ViewportPickTarget                           target;
        std::optional<AABB>                          bounds;
        std::shared_ptr<const IViewportPickGeometry> geometry;
        uint64_t                                     visibilityMask = ~uint64_t{0};
        int32_t                                      priority       = 0;
        bool                                         selectable = true, occluder = true;
    };
    struct ViewportPickProxySnapshot {
        uint64_t                          revision = 0;
        ViewportPickStatus                status   = ViewportPickStatus::HIT;
        std::vector<ViewportPickProxy>    proxies;
        std::string                       message;
        std::vector<ViewportPickGeometry> geometries;
    };
    class IViewportPickProvider {
    public:
        virtual ~IViewportPickProvider() = default;
        virtual uint64_t Id() const      = 0;
        virtual bool     RequiresModuleLease() const
        {
            return false;
        }
        // Called only during host preparation; snapshots are immutable and contain owned data.
        virtual std::shared_ptr<const ViewportPickProxySnapshot> Snapshot() = 0;
    };
    struct ViewportPickRegistration {
        std::shared_ptr<void> scope;
        std::string           message;
        explicit              operator bool() const
        {
            return bool(scope);
        }
    };
    struct ViewportProxyLayout {
        std::array<Vector2, 6> points{};
        uint32_t               count = 0, color = 0;
        Uuid                   actor;
    };
    struct ViewportSelectionStyle {
        uint32_t visible = 0xFF148CFF, occluded = 0x40148CFF;
        float    width = 2.f, markerRadius = 6.f;
    };
    struct ViewportSelectionBounds {
        std::optional<AABB> bounds;
        bool                surface = false;
    };
    // Both overlay drawing and hit testing use this layout, in viewport logical coordinates.
    bool ProjectViewportPickProxy(const ViewportPickProxy &proxy, const EditorCamera &camera, Vector2 extent, ViewportProxyLayout &layout);
    bool HitViewportPickProxy(
        const ViewportPickProxy &proxy, const ViewportPickRequest &request, float &distance, float &screenDistance, std::optional<Vector3> &position);

    bool MakeViewportPickRequest(const EditorCamera  &camera,
                                 Vector2              point,
                                 Vector2              extent,
                                 uint32_t             imageWidth,
                                 uint32_t             imageHeight,
                                 ViewportPickContext  context,
                                 ViewportPickRequest &result);
    bool SameViewportPickRequest(const ViewportPickRequest &a, const ViewportPickRequest &b);

    class IViewportPicker {
    public:
        virtual ~IViewportPicker() = default;
        // Host-thread queries only inspect prepared, owned data; they never load assets or build geometry.
        virtual ViewportPickResult Query(const ViewportPickRequest &request)  = 0;
        virtual ViewportPickResult Submit(const ViewportPickRequest &request) = 0;
        virtual ViewportPickResult Poll(const ViewportPickContext &token)     = 0;
        virtual void               Cancel(const ViewportPickContext &token)   = 0;
        // Dynamic module providers require a real ModuleManager::RetainModule lease.
        virtual ViewportPickRegistration RegisterProvider(std::shared_ptr<IViewportPickProvider>, std::shared_ptr<const void> moduleLease = {})
        {
            return {{}, "This picker does not support providers"};
        }
        virtual std::vector<ViewportProxyLayout> Overlay(const EditorCamera &, Vector2) const
        {
            return {};
        }
        virtual ViewportSelectionBounds SelectionBounds(const Uuid &) const
        {
            return {};
        }
        virtual ViewportPickContext Context() const           = 0;
        virtual uint64_t            PreparationSerial() const = 0;
    };
} // namespace sky::editor

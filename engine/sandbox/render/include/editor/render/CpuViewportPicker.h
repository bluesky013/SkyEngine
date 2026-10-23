//
// Created on 2026/10/23.
//

#pragma once

#include <aurora/adaptor/renderer/RenderSceneBridge.h>
#include <editor/core/viewport/IViewportPicker.h>
#include <memory>

namespace sky::editor {
    struct ViewportPickStatistics {
        uint64_t builds = 0, cacheHits = 0, evictions = 0, discarded = 0;
        uint64_t bytes = 0, reservedBytes = 0, triangles = 0, degenerateTriangles = 0;
        double   preparationMilliseconds = 0, queryMilliseconds = 0;
        double   decodeMilliseconds = 0, boundsMilliseconds = 0, partitionMilliseconds = 0, maintenanceMilliseconds = 0;
        double   allocationMilliseconds = 0, publicationMilliseconds = 0;
        uint64_t peakMeshBytes     = 0;
        uint64_t projectionUpdates = 0, proxyNodeVisits = 0;
        uint64_t indexUpdates = 0, inverseUpdates = 0, nodeVisits = 0, candidateTests = 0, triangleTests = 0, indexBytes = 0;
    };

    class CpuViewportPicker final : public IViewportPicker {
    public:
        ViewportSelectionBounds SelectionBounds(const Uuid &) const override;
        explicit CpuViewportPicker(uint64_t byteBudget = 64ull * 1024 * 1024, size_t entryBudget = 128);
        ~CpuViewportPicker() override;
        void                             Reset();
        void                             Prepare(std::shared_ptr<const aurora::SceneGeometrySnapshot> scene, ViewportPickContext context);
        ViewportPickResult               Query(const ViewportPickRequest &request) override;
        ViewportPickResult               Submit(const ViewportPickRequest &request) override;
        ViewportPickResult               Poll(const ViewportPickContext &token) override;
        void                             Cancel(const ViewportPickContext &token) override;
        ViewportPickRegistration         RegisterProvider(std::shared_ptr<IViewportPickProvider> provider,
                                                          std::shared_ptr<const void>            moduleLease = {}) override;
        std::vector<ViewportProxyLayout> Overlay(const EditorCamera &camera, Vector2 extent) const override;
        ViewportPickContext              Context() const override;
        uint64_t                         PreparationSerial() const override;
        ViewportPickStatistics           Statistics() const;

    private:
        struct Impl;
        std::unique_ptr<Impl> impl;
    };
} // namespace sky::editor

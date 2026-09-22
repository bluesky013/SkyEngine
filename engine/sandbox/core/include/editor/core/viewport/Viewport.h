//
// Created on 2026/09/21.
//

#pragma once

#include <core/template/Flags.h>
#include <cstdint>
#include <string>
#include <utility>

namespace sky::editor {

    // Where a viewport's content comes from.
    enum class ViewContentSource : uint8_t {
        SCENE = 0,        // the edited world (in-process)
        ASSET,            // an asset preview (material/mesh/skeleton)
        GAME_PROCESS,     // a separate PIE process (shared texture)
    };

    // How the viewport is interacted with.
    enum class ViewInteraction : uint8_t {
        EDIT = 0,         // selection, gizmo, tools
        PREVIEW,          // orbit/auto camera only
        PLAY,             // game input
    };

    // How the viewport is presented.
    enum class ViewPresentation : uint8_t {
        TEXTURE = 0,      // offscreen target composited as a UI image (no swapchain)
        WINDOW,           // its own native window + swapchain
        SHARED_TEXTURE,   // a GPU-shared texture from another process (PIE)
    };

    enum class ViewportOverlayFlagBit : uint32_t {
        NONE     = 0x00000000,
        GIZMO    = 0x00000001,
        GRID     = 0x00000002,
        PROFILER = 0x00000004,
    };

    using ViewportOverlayFlags = Flags<ViewportOverlayFlagBit>;
    ENABLE_FLAG_BIT_OPERATOR(ViewportOverlayFlagBit)

    // Toolkit/render-independent viewport description. The viewport owns its
    // presentation; the content target is independent of it.
    struct ViewportDescriptor {
        std::string          id;
        std::string          title;
        ViewContentSource    contentSource = ViewContentSource::SCENE;
        ViewInteraction      interaction   = ViewInteraction::PREVIEW;
        ViewPresentation     presentation  = ViewPresentation::TEXTURE;
        ViewportOverlayFlags overlays;
        uint32_t             width  = 0;
        uint32_t             height = 0;
    };

    // Common presets.
    namespace ViewportPresets {

        inline ViewportDescriptor EditorViewport(std::string id = "viewport")
        {
            ViewportDescriptor desc;
            desc.id           = std::move(id);
            desc.title        = "Viewport";
            desc.contentSource = ViewContentSource::SCENE;
            desc.interaction   = ViewInteraction::EDIT;
            desc.presentation  = ViewPresentation::TEXTURE;
            desc.overlays      = ViewportOverlayFlagBit::GIZMO | ViewportOverlayFlagBit::GRID;
            return desc;
        }

        inline ViewportDescriptor Preview(std::string id, std::string title = {})
        {
            ViewportDescriptor desc;
            desc.id           = std::move(id);
            desc.title        = title.empty() ? desc.id : std::move(title);
            desc.contentSource = ViewContentSource::SCENE;
            desc.interaction   = ViewInteraction::PREVIEW;
            desc.presentation  = ViewPresentation::TEXTURE;
            return desc;
        }

    } // namespace ViewportPresets

} // namespace sky::editor

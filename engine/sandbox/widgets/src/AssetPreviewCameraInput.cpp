#include <editor/widgets/AssetPreviewCameraInput.h>

#include <algorithm>
#include <cmath>
#include <core/math/MathUtil.h>
#include <editor/core/input/KeyModifiers.h>
#include <editor/core/viewport/EditorCamera.h>
#include <framework/window/IWindowEvent.h>
#include <ui/UIEvent.h>
#include <ui/UIRect.h>

namespace sky::editor {
    namespace {
        void ZoomOrbit(EditorCamera &camera, float amount)
        {
            const float nearest  = 2.f * camera.nearPlane;
            const float farthest = std::max(nearest, camera.farPlane * .9f);
            camera.orbitDistance = std::clamp(camera.orbitDistance * std::exp(-std::clamp(amount, -16.f, 16.f)), nearest, farthest);
            camera.Focus(camera.pivot);
        }
    } // namespace

    bool AssetPreviewCameraInput::Handle(EditorCamera &camera, const ui::UIPointerEvent &event, const ui::UIRect &image)
    {
        if (!std::isfinite(event.x) || !std::isfinite(event.y))
            return false;
        if (event.action == ui::UIPointerAction::WHEEL) {
            if (Captured() || !image.Contains(event.x, event.y) || !std::isfinite(event.wheelDelta) || event.wheelDelta == 0.f)
                return false;
            // Window input reports 120 units per detent. Keep the orbit pivot fixed throughout a zoom.
            ZoomOrbit(camera, std::clamp(event.wheelDelta / 120.f, -32.f, 32.f) * .16f);
            return true;
        }
        if (event.action == ui::UIPointerAction::DOWN) {
            if (Captured() || !image.Contains(event.x, event.y))
                return false;
            const bool alt = (event.modifiers & kModAlt) != 0 && (event.modifiers & (kModCtrl | kModShift)) == 0;
            if (event.button == static_cast<uint32_t>(MouseButtonType::MIDDLE))
                drag = Drag::Pan;
            else if (event.button == static_cast<uint32_t>(MouseButtonType::RIGHT))
                drag = alt ? Drag::Dolly : Drag::Orbit;
            else if (alt && event.button == static_cast<uint32_t>(MouseButtonType::LEFT))
                drag = Drag::Orbit;
            else
                return false;
            button   = event.button;
            pointer  = event.pointerId;
            previous = {event.x, event.y};
            return true;
        }
        if (!Captured() || event.pointerId != pointer || (event.action == ui::UIPointerAction::UP && event.button != button))
            return false;
        const Vector2 current{event.x, event.y};
        const auto    delta = current - previous;
        if (drag == Drag::Orbit)
            camera.Look(delta.x, delta.y, ToRadian(.2f), true);
        else if (drag == Drag::Pan)
            camera.Pan(-delta.x, -delta.y, image.Height());
        else
            ZoomOrbit(camera, (delta.x + delta.y) * 2.f / std::max(1.f, image.Height()));
        previous = current;
        if (event.action == ui::UIPointerAction::UP)
            Cancel();
        return true;
    }

    bool AssetPreviewCameraInput::IsFrameKey(const ui::UIKeyEvent &event)
    {
        return event.action == ui::UIKeyAction::DOWN && event.modifiers == 0 && event.keyCode == 'F';
    }
} // namespace sky::editor

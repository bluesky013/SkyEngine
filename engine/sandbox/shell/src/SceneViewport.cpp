//
// Created on 2026/10/22.
//

#include <editor/shell/SceneViewport.h>

#include <algorithm>
#include <cmath>
#include <editor/ui/UiTheme.h>
#include <ui/UIPaintContext.h>

namespace sky::editor {
    SceneViewport::SceneViewport(std::shared_ptr<ViewportController> controller) : controller(std::move(controller))
    {
        SetFocusable(true);
    }
    SceneViewport::~SceneViewport()
    {
        controller->Cancel();
    }
    const char *SceneViewport::GetTypeName() const
    {
        return "SceneViewport";
    }
    GizmoStyle SceneViewport::Style() const
    {
        const auto &theme = GetDefaultUiTheme();
        return {theme.metrics.gizmoLength,    theme.metrics.gizmoWidth,
                theme.metrics.gizmoHitRadius, {theme.colors.gizmoX, theme.colors.gizmoY, theme.colors.gizmoZ},
                theme.colors.white,           theme.colors.gizmoHighlight};
    }
    void SceneViewport::ArrangeChildren(const ui::UIRect &content)
    {
        Image::ArrangeChildren(content);
        if (content.left != contentRect.left || content.top != contentRect.top || content.right != contentRect.right ||
            content.bottom != contentRect.bottom)
            controller->InvalidatePickLayout();
        contentRect = content;
        if (extentChanged)
            extentChanged({content.Width(), content.Height()});
    }
    void SceneViewport::OnPaint(ui::UIPaintContext &paint)
    {
        SyncPickScale();
        Image::OnPaint(paint);
        const Vector2 proxyOrigin{contentRect.left, contentRect.top};
        const auto   &theme    = GetDefaultUiTheme();
        const auto    selected = controller->SelectedActors();
        paint.PushClip(contentRect);
        for (const auto &layout : controller->PickOverlay({contentRect.Width(), contentRect.Height()})) {
            const auto color = std::ranges::find(selected, layout.actor) != selected.end() ? theme.colors.selectionOutline : layout.color;
            for (uint32_t i = 1; i + 1 < layout.count; ++i) {
                const auto a = layout.points[0] + proxyOrigin, b = layout.points[i] + proxyOrigin, c = layout.points[i + 1] + proxyOrigin;
                paint.AddTriangle(a.x, a.y, b.x, b.y, c.x, c.y, color);
            }
        }
        paint.PopClip();
        GizmoGeometry geometry;
        const auto    style = Style();
        controller->BuildSelectionGeometry(
            {contentRect.Width(), contentRect.Height()},
            {theme.colors.selectionOutline, theme.colors.selectionOccluded, theme.metrics.selectionWidth, theme.metrics.selectionMarkerRadius},
            geometry);
        GizmoGeometry transform;
        if (controller->BuildGeometry({contentRect.Width(), contentRect.Height()}, style, transform))
            geometry.insert(geometry.end(), transform.begin(), transform.end());
        const Vector2 origin{contentRect.left, contentRect.top};
        paint.PushClip(contentRect);
        const auto triangle = [&](Vector2 a, Vector2 b, Vector2 c, uint32_t color) {
            a += origin;
            b += origin;
            c += origin;
            paint.AddTriangle(a.x, a.y, b.x, b.y, c.x, c.y, color);
        };
        for (const auto &shape : geometry) {
            const bool highlight = shape.handle == controller->ActiveHandle() || (!controller->HasGesture() && shape.handle == hover);
            const auto color     = highlight && shape.handle != GizmoHandle::NONE ? style.highlight : shape.color;
            if (shape.filled) {
                for (size_t i = 1; i + 1 < shape.points.size(); ++i)
                    triangle(shape.points[0], shape.points[i], shape.points[i + 1], color);
            } else {
                const size_t count = shape.points.size();
                for (size_t i = 0; i < (shape.closed ? count : count - 1); ++i) {
                    const auto  a = shape.points[i], b = shape.points[(i + 1) % count];
                    const auto  delta  = b - a;
                    const float length = delta.Length();
                    if (length < 1.e-5f)
                        continue;
                    const Vector2 normal = Vector2{-delta.y, delta.x} * (shape.width * .5f / length);
                    triangle(a + normal, b + normal, b - normal, color);
                    triangle(a + normal, b - normal, a - normal, color);
                }
            }
        }
        paint.PopClip();
    }
    ui::UIEventResult SceneViewport::OnPointerEvent(const ui::UIPointerEvent &event)
    {
        SyncPickScale();
        using ui::UIEventResult;
        using ui::UIPointerAction;
        if (!controller->IsEditing())
            return UIEventResult::UNHANDLED;
        const Vector2 point{event.x - contentRect.left, event.y - contentRect.top};
        const Vector2 extent{contentRect.Width(), contentRect.Height()};
        const float   threshold = GetDefaultUiTheme().metrics.viewportClickDistance;
        if (controller->IsClickCandidate()) {
            if (event.pointerId != pointerId)
                return UIEventResult::UNHANDLED;
            if (threshold != clickThreshold || event.action == UIPointerAction::WHEEL ||
                (event.action == UIPointerAction::DOWN && event.button != button)) {
                controller->Cancel();
                return UIEventResult::HANDLED;
            }
            clickExceeded |= (point - clickStart).Length() > clickThreshold;
            if (event.action == UIPointerAction::UP && event.button == button) {
                if (!clickExceeded && contentRect.Contains(event.x, event.y))
                    controller->FinishClick(point, extent);
                else
                    controller->Cancel();
            }
            return UIEventResult::HANDLED;
        }
        if (controller->IsNavigating() || controller->ActiveHandle() != GizmoHandle::NONE) {
            if (event.pointerId != pointerId)
                return UIEventResult::UNHANDLED;
            if (event.action == UIPointerAction::MOVE)
                controller->MovePointer(point);
            if (event.action == UIPointerAction::UP && event.button == button) {
                controller->MovePointer(point);
                controller->Release();
            }
            MarkPaintDirty();
            return UIEventResult::HANDLED;
        }
        if (!contentRect.Contains(event.x, event.y))
            return UIEventResult::UNHANDLED;
        if (event.action == UIPointerAction::WHEEL) {
            controller->Cancel();
            controller->settings.Normalize();
            controller->camera.Dolly(event.wheelDelta / 120.f * controller->settings.speed * .1f);
            return UIEventResult::HANDLED;
        }
        GizmoGeometry geometry;
        const auto    style = Style();
        controller->BuildGeometry(extent, style, geometry);
        hover = HitTestGizmo(geometry, point, style.hitRadius);
        if (event.action == UIPointerAction::DOWN) {
            pointerId = event.pointerId;
            button    = event.button;
            if (event.button == 1)
                controller->BeginNavigation(ViewportNavigation::LOOK, point, extent.y);
            else if (event.button == 2)
                controller->BeginNavigation(ViewportNavigation::PAN, point, extent.y);
            else if (event.button == 0 && orbitHeld && orbitHeld())
                controller->BeginNavigation(ViewportNavigation::ORBIT, point, extent.y);
            else if (event.button == 0 && hover != GizmoHandle::NONE)
                controller->BeginDrag(hover, point, extent, style);
            else if (event.button == 0) {
                clickStart     = point;
                clickThreshold = threshold;
                clickExceeded  = false;
                controller->BeginClick();
            }
            return UIEventResult::HANDLED;
        }
        MarkPaintDirty();
        return UIEventResult::UNHANDLED;
    }
    ui::UIEventResult SceneViewport::OnKeyEvent(const ui::UIKeyEvent &event)
    {
        return controller->NavigationKey(event.keyCode, event.action != ui::UIKeyAction::UP) ? ui::UIEventResult::HANDLED
                                                                                             : ui::UIEventResult::UNHANDLED;
    }
    void SceneViewport::OnPointerCancel(uint32_t id)
    {
        if (id == pointerId)
            controller->Cancel();
    }
    void SceneViewport::SyncPickScale()
    {
        const float scale = GetDefaultUiTheme().scale;
        if (pickScale != 0 && pickScale != scale)
            controller->InvalidatePickLayout();
        pickScale = scale;
    }
    void SceneViewport::OnPointerButtonConflict(uint32_t id)
    {
        if (id == pointerId && controller->IsClickCandidate())
            controller->Cancel();
    }
    bool SceneViewport::WantsPointerCapture() const
    {
        return controller->IsClickCandidate() || controller->IsNavigating() || controller->ActiveHandle() != GizmoHandle::NONE;
    }
} // namespace sky::editor

//
// Created on 2026/11/15.
//

#include <editor/widgets/TooltipHost.h>

#include <editor/widgets/UiSkin.h>
#include <ui/UIEventRouter.h>
#include <ui/UIPaintContext.h>
#include <ui/text/UITextLayout.h>
#include <ui/text/UITextSystem.h>

#include <algorithm>
#include <cmath>

namespace sky::editor {
    namespace {

        constexpr float HOVER_DELAY = 0.5f;

        bool SameToken(const std::weak_ptr<void> &a, const std::weak_ptr<void> &b)
        {
            return !a.owner_before(b) && !b.owner_before(a);
        }

        sky::ui::UIRect AncestorClip(const sky::ui::UIElement &element)
        {
            const auto *root = &element;
            while (root->GetParent() != nullptr) {
                root = root->GetParent();
            }
            auto clip = root->GetBounds();
            for (const auto *parent = element.GetParent(); parent != nullptr; parent = parent->GetParent()) {
                if (parent->ClipsChildren()) {
                    clip = sky::ui::UIRect::Intersect(clip, parent->GetGeometry().SurfaceBounds());
                }
            }
            return clip;
        }

        struct Candidate {
            sky::ui::UIElement::InputReference owner;
            sky::ui::UITooltipTarget           target;
            sky::ui::UIRect                    anchor;
        };

        bool QueryCandidate(const sky::ui::UIElement &element, const sky::ui::UIPoint &pointer, Candidate &candidate)
        {
            const auto       clip = element.ClipsChildren() ? sky::ui::UIRect::Intersect(AncestorClip(element), element.GetGeometry().SurfaceBounds())
                                                            : AncestorClip(element);
            const auto       geometry = element.GetGeometry();
            sky::ui::UIPoint local;
            if (!clip.Contains(pointer.x, pointer.y) || !geometry.SurfaceToLayout(pointer.x, pointer.y, local))
                return false;
            const auto               reference = element.GetInputReference();
            sky::ui::UITooltipTarget target;
            if (!element.QueryTooltip(local.x, local.y, target) || !reference.Get() || target.lifetime.expired())
                return false;
            const auto anchor = sky::ui::UIRect::Intersect(clip, sky::ui::TransformRect(target.anchor, geometry.layoutToSurface));
            if (anchor.IsEmpty())
                return false;
            candidate = {reference, target, anchor};
            return true;
        }

        bool FindTooltip(sky::ui::UIElement &root, const sky::ui::UIPoint &pointer, bool captured, Candidate &candidate)
        {
            auto *hit = sky::ui::UIEventRouter::HitTest(root, pointer.x, pointer.y);
            if (!hit && captured)
                hit = &root;
            while (hit) {
                const auto reference = hit->GetInputReference();
                if (QueryCandidate(*hit, pointer, candidate))
                    return true;
                if (!reference.Get() || hit == &root)
                    break;
                hit = hit->GetParent();
            }
            return false;
        }

    } // namespace

    void TooltipHost::ClearTarget()
    {
        owner  = {};
        target = {};
        anchor = {};
        text.clear();
        layout         = {};
        layoutAtlas    = nullptr;
        pointerChanged = true;
        elapsed = 0.0f;
        built   = false;
    }

    void TooltipHost::Cancel()
    {
        ClearTarget();
        pointerActive = false;
    }

    void TooltipHost::Reset()
    {
        Cancel();
        rootReference = {};
        pressedButtons.clear();
    }

    void TooltipHost::Pointer(const sky::ui::UIPointerEvent &event)
    {
        if (event.pointerId != 0) {
            return;
        }
        if (!built && (pointer.x != event.x || pointer.y != event.y)) {
            elapsed = 0.0f;
        }
        pointerChanged = pointerChanged || pointer.x != event.x || pointer.y != event.y;
        pointer = {event.x, event.y};
        if (event.action == sky::ui::UIPointerAction::DOWN) {
            if (std::find(pressedButtons.begin(), pressedButtons.end(), event.button) == pressedButtons.end()) {
                pressedButtons.push_back(event.button);
            }
        } else if (event.action == sky::ui::UIPointerAction::UP) {
            std::erase(pressedButtons, event.button);
        }
        pointerActive = event.action == sky::ui::UIPointerAction::MOVE && pressedButtons.empty() && std::isfinite(event.x) && std::isfinite(event.y);
        if (!pointerActive) {
            ClearTarget();
        }
    }

    bool TooltipHost::Unchanged(const TooltipInteractionScope &scope) const
    {
        return !pointerChanged && rootReference.Get() == scope.root && captureReference.Get() == scope.captureRoot &&
               previousScope.layoutRevision == scope.layoutRevision && previousScope.overlayRevision == scope.overlayRevision &&
               previousScope.themeRevision == scope.themeRevision;
    }

    void TooltipHost::Tick(sky::ui::UIElement *root, float delta)
    {
        Tick({root, nullptr, root ? root->GetLayoutRevision() : 0, root ? root->GetPaintRevision() : 0, GetDefaultUiThemeRevision()}, delta);
    }

    void TooltipHost::Tick(const TooltipInteractionScope &scope, float delta)
    {
        auto *root = scope.root;
        if (!pointerActive || root == nullptr || !root->IsEffectivelyVisible()) {
            ClearTarget();
            rootReference = {};
            return;
        }
        if (rootReference.Get() != root) {
            ClearTarget();
            rootReference = root->GetInputReference();
        }
        if (built && Unchanged(scope) && owner.Get() && owner.Get()->IsEffectivelyVisible() && !target.lifetime.expired())
            return;
        auto     *capture    = scope.captureRoot;
        auto     *searchRoot = capture != nullptr ? capture : root;
        Candidate candidate;
        if (!FindTooltip(*searchRoot, pointer, capture != nullptr, candidate)) {
            ClearTarget();
            return;
        }
        if (owner.Get() != candidate.owner.Get() || target.id != candidate.target.id || !SameToken(target.lifetime, candidate.target.lifetime)) {
            ClearTarget();
            owner  = candidate.owner;
            target = candidate.target;
        }
        anchor = candidate.anchor;
        previousScope    = scope;
        captureReference = capture ? capture->GetInputReference() : sky::ui::UIElement::InputReference{};
        pointerChanged   = false;
        if (built) {
            return;
        }
        if (std::isfinite(delta) && delta > 0.0f) {
            elapsed += delta;
        }
        if (elapsed < HOVER_DELAY) {
            return;
        }
        built = true;
        std::string value;
        const auto *element = owner.Get();
        if (element != nullptr && element->BuildTooltip(target.id, value) && owner.Get() != nullptr && !target.lifetime.expired()) {
            text = std::move(value);
        }
    }

    bool TooltipHost::IsVisible() const
    {
        return !text.empty() && rootReference.Get() && rootReference.Get()->IsEffectivelyVisible() && owner.Get() &&
               owner.Get()->IsEffectivelyVisible() && !target.lifetime.expired();
    }

    void TooltipHost::ValidateRoot(sky::ui::UIElement *root)
    {
        ValidateRoot({root, nullptr, root ? root->GetLayoutRevision() : 0, root ? root->GetPaintRevision() : 0, GetDefaultUiThemeRevision()});
    }

    void TooltipHost::ValidateRoot(const TooltipInteractionScope &scope)
    {
        if (!owner.Get() || !scope.root || !scope.root->IsEffectivelyVisible() || !owner.Get()->IsEffectivelyVisible() || target.lifetime.expired()) {
            ClearTarget();
            return;
        }
        if (Unchanged(scope))
            return;
        Candidate candidate;
        auto     *root = scope.captureRoot ? scope.captureRoot : scope.root;
        if (rootReference.Get() != scope.root || !FindTooltip(*root, pointer, scope.captureRoot != nullptr, candidate) ||
            owner.Get() != candidate.owner.Get() || target.id != candidate.target.id || !SameToken(target.lifetime, candidate.target.lifetime)) {
            ClearTarget();
            return;
        }
        anchor = candidate.anchor;
        previousScope    = scope;
        captureReference = scope.captureRoot ? scope.captureRoot->GetInputReference() : sky::ui::UIElement::InputReference{};
        pointerChanged   = false;
    }

    void TooltipHost::Paint(sky::ui::UIPaintContext &context, const sky::ui::UIRect &surfaceBounds, const UiSkin &skin)
    {
        if (!IsVisible() || skin.Text() == nullptr) {
            return;
        }
        const auto           &theme   = skin.Theme();
        const auto           &metrics = theme.metrics;
        const float           margin  = metrics.panelGap;
        const float           padding = metrics.controlPad;
        const sky::ui::UIRect area{surfaceBounds.left + margin, surfaceBounds.top + margin, surfaceBounds.right - margin,
                                   surfaceBounds.bottom - margin};
        const float           maxWidth = std::min(metrics.tooltipMaxWidth, area.Width()) - 2.0f * padding;
        if (area.IsEmpty() || !std::isfinite(maxWidth) || maxWidth <= 0.0f || area.Height() <= 2.0f * padding) {
            return;
        }
        auto      &atlas  = skin.Text()->GetAtlas();
        if (layoutAtlas != &atlas || layoutWidth != maxWidth || layout.fontSize != theme.fonts.label || layoutTheme != GetDefaultUiThemeRevision()) {
            layout      = sky::ui::UITextLayout::Build(text, theme.fonts.label, atlas, maxWidth, true);
            layoutAtlas = &atlas;
            layoutWidth = maxWidth;
            layoutTheme = GetDefaultUiThemeRevision();
        }
        if (layout.lines.empty()) {
            return;
        }
        const float width  = std::min(area.Width(), layout.extent.width + 2.0f * padding);
        const float height = std::min(area.Height(), layout.extent.height + 2.0f * padding);
        const float left   = std::clamp(anchor.left, area.left, area.right - width);
        float       top    = anchor.bottom + margin;
        if (top + height > area.bottom) {
            top = anchor.top - margin - height;
        }
        top = std::clamp(top, area.top, area.bottom - height);
        const sky::ui::UIRect bounds{left, top, left + width, top + height};
        context.PushClip(surfaceBounds);
        skin.DrawPopup(context, bounds);
        context.PushClip({bounds.left + padding, bounds.top + padding, bounds.right - padding, bounds.bottom - padding});
        sky::ui::UITextLayout::Emit(context, layout, bounds.left + padding, bounds.top + padding, theme.colors.text, atlas);
        context.PopClip();
        context.PopClip();
    }

} // namespace sky::editor

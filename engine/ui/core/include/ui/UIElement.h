//
// Created on 2026/09/19.
//

#pragma once

#include <ui/UIEvent.h>
#include <ui/UIGeometry.h>
#include <ui/UILayout.h>
#include <ui/UIRect.h>
#include <ui/UITransform.h>
#include <ui/data/UIProperty.h>

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace sky::ui {

    class UIElement;
    class UIContext;
    class UIPaintContext;
    using UIElementPtr = std::unique_ptr<UIElement>;

    class UIElement {
    public:
        UIElement();
        virtual ~UIElement();

        UIElement(const UIElement &)            = delete;
        UIElement &operator=(const UIElement &) = delete;

        // Non-owning snapshot of an element and its current tree attachment.
        // Removing/readding the same object invalidates an in-flight route.
        struct InputReference {
            UIElement          *element = nullptr;
            std::weak_ptr<void> lifetime;
            std::weak_ptr<void> attachment;
            UIElement          *GetAlive() const
            {
                return !lifetime.expired() ? element : nullptr;
            }
            UIElement *Get() const
            {
                return !attachment.expired() ? GetAlive() : nullptr;
            }
        };
        InputReference GetInputReference() const;

        uint32_t GetId() const
        {
            return id;
        }

        const std::string &GetName() const
        {
            return name;
        }
        void SetName(const std::string &value)
        {
            name = value;
        }

        UIElement *GetParent() const
        {
            return parent;
        }

        UIElement   *AddChild(UIElementPtr child);
        UIElementPtr RemoveChild(UIElement *child);
        void         ClearChildren();
        // Reorders a direct child among its siblings; delta is the index shift.
        bool MoveChild(UIElement *child, int delta);

        // Document type key (overridden by concrete widgets).
        virtual const char *GetTypeName() const
        {
            return "UIElement";
        }
        const std::vector<UIElementPtr> &GetChildren() const
        {
            return children;
        }

        // True when this element currently owns the pointer beyond its own bounds (e.g. an open popup),
        // so a host routing input to children should deliver to it regardless of hit-testing.
        virtual bool WantsPointerCapture() const
        {
            return false;
        }

        void SetVisible(bool value);
        bool IsVisible() const
        {
            return visible;
        }
        bool IsEffectivelyVisible() const;

        void SetEnabled(bool value)
        {
            if (enabled != value) {
                enabled = value;
                MarkPaintDirty();
            }
        }
        bool IsEnabled() const
        {
            return enabled;
        }

        void          SetBounds(const UIRect &value);
        const UIRect &GetBounds() const
        {
            return bounds;
        }
        // Final logical allocation, preserved before positioning float edges.
        // Set only by SetBounds/Arrange/Layout, never by a measurement query.
        UISize GetArrangedSize() const
        {
            return arrangedSize;
        }
        UISize GetContentSize() const
        {
            return layout.ContentSize(arrangedSize);
        }

        // Layout bounds remain in the untransformed surface coordinate system.
        // This snapshot includes its context's surface transform and composes
        // all ancestor render transforms on demand. The optional argument is
        // an additional transform applied outside that surface mapping.
        UIGeometry GetGeometry(const UI2DTransform &surfaceTransform = {}) const;

        // Input changes invalidate self and ancestors, without touching siblings
        // or descendants. Changed final rectangles propagate during arrangement.
        void MarkLayoutDirty();
        // Allocation-only changes (e.g. scrolling) preserve desired-size caches.
        void MarkArrangeDirty();
        void MarkPaintDirty();

        // Legacy/custom elements may poll external state in layout callbacks.
        // They remain volatile by default; controlled widgets opt into retention.
        void SetLayoutVolatile(bool value);
        bool IsLayoutVolatile() const
        {
            return layoutVolatile;
        }

        void ClearLayoutDirty()
        {
            layoutDirty = false;
        }
        void ClearPaintDirty()
        {
            paintDirty = false;
        }
        bool IsLayoutDirty() const
        {
            return layoutDirty;
        }
        bool IsPaintDirty() const
        {
            return paintDirty;
        }

        void                  SetLayout(const UILayoutParams &value);
        const UILayoutParams &GetLayout() const
        {
            return layout;
        }

        // Children ordered by ascending z (stable for equal z). Paint walks this
        // order; hit-testing walks it in reverse.
        std::vector<UIElement *> GetPaintOrder() const;

        void                            AddStyleClass(const std::string &className);
        const std::vector<std::string> &GetStyleClasses() const
        {
            return styleClasses;
        }

        // Legacy hook for classes inheriting this default OnMeasure. The base
        // has no intrinsic size. Migrated native widgets make this wrapper
        // final: subclasses of those widgets must override OnMeasure instead.
        virtual void Measure(float &outWidth, float &outHeight);
        // Side-effect-free outer desired size under an available-space budget.
        // Does not apply this element's layout rules or change resolved bounds.
        UISize MeasureDesired(const UIConstraints &constraints = {});
        // Applies this element's size rules once before measuring. Slot owners
        // use MeasureDesired when they have already allocated the final axes.
        UISize MeasureForLayout(const UIConstraints &available = {});
        // Resolves legacy anchor/size rules against a parent rect, then arranges.
        void Layout(const UIRect &parentRect);
        void Layout(const UIRect &parentRect, UISize parentSize);
        // Accepts the final rect assigned by a parent or host and recursively
        // arranges the subtree. Paint and hit-testing consume these bounds.
        void Arrange(const UIRect &finalRect);
        // Slot owners preserve their pre-positioning allocation explicitly.
        // The rect-only overload instead treats endpoint differences as exact.
        void Arrange(const UIRect &finalRect, UISize logicalSize);
        // Incremental entry points. During this pass, descendant Layout/Arrange
        // calls skip clean retained branches with unchanged inputs. Explicit
        // Layout/Arrange outside a pass continue to execute their callbacks.
        void LayoutIfNeeded(const UIRect &parentRect);
        void LayoutIfNeeded(const UIRect &parentRect, UISize parentSize);
        void ArrangeIfNeeded(const UIRect &finalRect);
        void ArrangeIfNeeded(const UIRect &finalRect, UISize logicalSize);

    protected:
        struct InputCancellationState {
            uint32_t pointerId;
            bool     delivered = false;
        };
        struct InputCancellation {
            InputReference                          target;
            uint32_t                                pointerId;
            std::shared_ptr<InputCancellationState> delivery;
        };
        // Pure collection: clear direct-routing state without invoking widgets.
        // A null child means this entire container is being detached.
        virtual void CollectDetachedInput(UIElement *child, std::vector<InputCancellation> &out)
        {
        }
        // Returns outer desired size, including this element's padding once.
        // The default bridges only to the legacy virtual Measure overload.
        // Override this hook below native Text/container classes. Their final
        // legacy wrapper routes both old and new queries through this hook.
        virtual UISize OnMeasure(const UIConstraints &constraints);
        // Arranges this element's children inside its content rect. Containers
        // override this; the base arranges every child against the same rect.
        virtual void ArrangeChildren(const UIRect &content);
        // Child metadata must follow the live tree, including inherited
        // RemoveChild/ClearChildren/MoveChild operations.
        virtual void OnChildrenChanged()
        {
        }

    public:
        virtual void OnPaint(UIPaintContext &context)
        {
            (void)context;
        }
        void Paint(UIPaintContext &context);

        virtual UIEventResult OnPointerEvent(const UIPointerEvent &event)
        {
            (void)event;
            return UIEventResult::UNHANDLED;
        }
        // Capture/hover can become unmappable after a singular transform.
        // Cancel without inventing coordinates; interactive widgets reset here.
        virtual void OnPointerCancel(uint32_t pointerId)
        {
            (void)pointerId;
        }
        virtual void OnPointerButtonConflict(uint32_t pointerId)
        {
            (void)pointerId;
        }
        virtual UIEventResult OnKeyEvent(const UIKeyEvent &event)
        {
            (void)event;
            return UIEventResult::UNHANDLED;
        }
        virtual UIEventResult OnTextInput(const UITextInputEvent &event)
        {
            (void)event;
            return UIEventResult::UNHANDLED;
        }
        virtual void OnPointerEnter(const UIPointerEvent &event)
        {
            (void)event;
        }
        virtual void OnPointerLeave(const UIPointerEvent &event)
        {
            (void)event;
        }

        void SetFocusable(bool value)
        {
            focusable = value;
        }
        bool IsFocusable() const
        {
            return focusable;
        }

        void SetClipsChildren(bool value)
        {
            if (clipsChildren != value) {
                clipsChildren = value;
                MarkPaintDirty();
            }
        }
        bool ClipsChildren() const
        {
            return clipsChildren;
        }

        void               SetTransform(const UITransform &value);
        const UITransform &GetTransform() const
        {
            return transform;
        }

        void  SetOpacity(float value);
        float GetOpacity() const
        {
            return opacity;
        }

        // Generic property path applied by the document loader (e.g. "visible",
        // "visual.texture"). Widgets override to extend. Returns false if unknown.
        virtual bool SetProperty(const std::string &path, const UIPropertyValue &value);

    private:
        friend class UIContext;
        friend class UIEventRouter;
        static uint32_t nextId;

        void        RenewInputAttachment();
        void        CollectSubtreeInput(std::vector<InputCancellation> &out);
        static void QueueDetachedInput(std::vector<InputCancellation> &pending);
        static void CancelDetachedInput(const std::vector<InputCancellation> &pending);
        UIContext  *GetInputContext() const;

        std::shared_ptr<void>                              inputLifetime   = std::make_shared<char>();
        std::shared_ptr<void>                              inputAttachment = std::make_shared<char>();
        UIContext                                         *inputContext    = nullptr; // Only set on a context's root.
        std::weak_ptr<void>                                contextLifetime;
        std::vector<std::weak_ptr<InputCancellationState>> pendingInputCancellations;

        struct MeasureCacheEntry {
            UIConstraints constraints;
            UISize        size;
            bool          valid = false;
        };
        class LayoutPass;
        UIElement *GetTreeRoot();
        void       AdjustVolatileCount(size_t count, bool add);
        bool       CanRetainLayout() const
        {
            return volatileCount == 0;
        }

        uint32_t                  id = 0;
        std::string               name;
        UIElement                *parent = nullptr;
        std::vector<UIElementPtr> children;
        UIRect                    bounds;
        UISize                    arrangedSize;
        UILayoutParams            layout;
        std::vector<std::string>  styleClasses;
        bool                      visible       = true;
        bool                      enabled       = true;
        bool                      clipsChildren = false;
        bool                      focusable     = false;
        UITransform               transform;
        // Owned by UIContext at its root; one source for paint and input.
        UI2DTransform                    surfaceTransform;
        float                            opacity           = 1.0f;
        bool                             layoutDirty       = true;
        bool                             paintDirty        = true;
        bool                             layoutVolatile    = true;
        size_t                           volatileCount     = 1;
        bool                             incrementalLayout = false;
        bool                             arranged          = false;
        bool                             hasLayoutInput    = false;
        UIRect                           lastLayoutInput;
        UISize                           lastLayoutInputSize;
        UIRect                           lastLayoutResult;
        UISize                           lastLayoutResultSize;
        uint64_t                         layoutGeneration  = 0;
        uint64_t                         measureGeneration = 0;
        uint64_t                         paintGeneration   = 0;
        std::array<MeasureCacheEntry, 4> measureCache;
        size_t                           nextMeasureEntry = 0;
    };

} // namespace sky::ui

//
// Geometry, pointer routing and surface transforms share the same coordinate contract.
//

#include <gtest/gtest.h>
#include <ui/UIContext.h>
#include <ui/UIElement.h>
#include <ui/UIEventRouter.h>
#include <ui/UIGeometry.h>
#include <ui/UIPaintContext.h>
#include <ui/detail/ChildRouting.h>
#include <ui/widgets/Button.h>
#include <ui/widgets/Overlay.h>

#include <limits>
#include <vector>

namespace sky::ui {

    namespace {

        constexpr float kHalfPi    = 1.5707963267948966f;
        constexpr float kQuarterPi = 0.7853981633974483f;
        constexpr float kTolerance = 0.0001f;

        class PointerProbe : public UIElement {
        public:
            std::vector<UIPointerEvent> received;
            std::vector<UIPointerEvent> entered;
            std::vector<UIPointerEvent> left;
            std::vector<uint32_t>       canceled;
            bool                        handled = true;

            UIEventResult OnPointerEvent(const UIPointerEvent &event) override
            {
                received.push_back(event);
                return handled ? UIEventResult::HANDLED : UIEventResult::UNHANDLED;
            }
            void OnPointerEnter(const UIPointerEvent &event) override
            {
                entered.push_back(event);
            }
            void OnPointerLeave(const UIPointerEvent &event) override
            {
                left.push_back(event);
            }
            void OnPointerCancel(uint32_t pointerId) override
            {
                canceled.push_back(pointerId);
            }
        };

        class ForwardingOverlay : public Overlay {
        public:
            std::vector<uint32_t> canceled;

            UIEventResult OnPointerEvent(const UIPointerEvent &event) override
            {
                return RoutePointer(event);
            }
            void OnPointerCancel(uint32_t pointerId) override
            {
                canceled.push_back(pointerId);
                Overlay::OnPointerCancel(pointerId);
            }
        };

        UIPointerEvent Pointer(UIPointerAction action, float x, float y)
        {
            UIPointerEvent event;
            event.action = action;
            event.x      = x;
            event.y      = y;
            return event;
        }

        void ExpectPoint(const UIPointerEvent &event, float x, float y)
        {
            EXPECT_NEAR(event.x, x, kTolerance);
            EXPECT_NEAR(event.y, y, kTolerance);
        }

        void ExpectRect(const UIRect &rect, float left, float top, float right, float bottom)
        {
            EXPECT_NEAR(rect.left, left, kTolerance);
            EXPECT_NEAR(rect.top, top, kTolerance);
            EXPECT_NEAR(rect.right, right, kTolerance);
            EXPECT_NEAR(rect.bottom, bottom, kTolerance);
        }

    } // namespace

    TEST(UIGeometryTest, InverseAndTransformedBoundsHandleReflectionAndTinyScale)
    {
        UI2DTransform matrix;
        matrix.m00 = 0.0f;
        matrix.m01 = -2.0f;
        matrix.m02 = 100.0f;
        matrix.m10 = -3.0f;
        matrix.m11 = 0.0f;
        matrix.m12 = 200.0f;
        ExpectRect(TransformRect({10.0f, 20.0f, 30.0f, 50.0f}, matrix), 0.0f, 110.0f, 60.0f, 170.0f);
        UI2DTransform inverse;
        ASSERT_TRUE(matrix.TryInverse(inverse));
        float x = 17.0f;
        float y = 36.0f;
        matrix.Apply(x, y);
        inverse.Apply(x, y);
        EXPECT_NEAR(x, 17.0f, kTolerance);
        EXPECT_NEAR(y, 36.0f, kTolerance);

        UI2DTransform tiny;
        tiny.m00 = 1.0e-20f;
        tiny.m11 = 1.0e-20f;
        EXPECT_TRUE(tiny.TryInverse(inverse));
        UI2DTransform singular;
        singular.m00 = 0.0f;
        inverse.m02  = 123.0f;
        EXPECT_FALSE(singular.TryInverse(inverse));
        EXPECT_FLOAT_EQ(inverse.m02, 123.0f);
        singular.m00 = std::numeric_limits<float>::quiet_NaN();
        EXPECT_FALSE(singular.TryInverse(inverse));
    }

    TEST(UIGeometryTest, GeometryAccumulatesAncestorsAndKeepsAbsoluteLayoutCoordinates)
    {
        UIElement   parent;
        UITransform parentTransform;
        parentTransform.translationX = 100.0f;
        parentTransform.translationY = 50.0f;
        parentTransform.scaleX       = 2.0f;
        parentTransform.scaleY       = 3.0f;
        parent.SetTransform(parentTransform);
        auto       childHolder = std::make_unique<UIElement>();
        UIElement *child       = parent.AddChild(std::move(childHolder));
        child->SetBounds({10.0f, 20.0f, 30.0f, 40.0f});
        UITransform childTransform;
        childTransform.translationX = 5.0f;
        childTransform.translationY = 7.0f;
        childTransform.rotation     = kHalfPi;
        child->SetTransform(childTransform);
        UI2DTransform compositor;
        compositor.m02            = 10.0f;
        compositor.m12            = 20.0f;
        const UIGeometry geometry = child->GetGeometry(compositor);
        ExpectRect(geometry.layoutBounds, 10.0f, 20.0f, 30.0f, 40.0f);
        ExpectRect(geometry.SurfaceBounds(), 40.0f, 121.0f, 80.0f, 181.0f);
        UIPoint layout;
        ASSERT_TRUE(geometry.SurfaceToLayout(72.0f, 127.0f, layout));
        EXPECT_NEAR(layout.x, 12.0f, kTolerance);
        EXPECT_NEAR(layout.y, 24.0f, kTolerance);
        EXPECT_TRUE(geometry.ContainsSurfacePoint(72.0f, 127.0f));
        EXPECT_FALSE(geometry.ContainsSurfacePoint(12.0f, 24.0f));
        ExpectRect(child->GetBounds(), 10.0f, 20.0f, 30.0f, 40.0f);
    }

    TEST(UIGeometryTest, NestedRotationAndScaleDeliverButtonClicksAtRenderedPosition)
    {
        UIContext context;
        context.SetContentSize(400.0f, 400.0f);
        UIElement *parent = context.AddChild(std::make_unique<UIElement>());
        parent->SetBounds({0.0f, 0.0f, 100.0f, 100.0f});
        UITransform parentTransform;
        parentTransform.translationX = 180.0f;
        parentTransform.translationY = 60.0f;
        parentTransform.rotation     = kHalfPi;
        parentTransform.scaleX       = 1.5f;
        parent->SetTransform(parentTransform);
        auto    buttonHolder = std::make_unique<Button>();
        Button *button       = buttonHolder.get();
        button->SetBounds({10.0f, 20.0f, 50.0f, 40.0f});
        UITransform childTransform;
        childTransform.translationX = 8.0f;
        childTransform.translationY = 4.0f;
        childTransform.scaleX       = 1.25f;
        childTransform.scaleY       = 0.75f;
        button->SetTransform(childTransform);
        parent->AddChild(std::move(buttonHolder));
        int clicks = 0;
        button->SetOnClick([&clicks]() { ++clicks; });
        UIEventRouter router(context);
        // Layout center (30,30) maps through child scale/translation and parent rotation.
        EXPECT_EQ(router.HitTest(153.5f, 128.25f), button);
        EXPECT_NE(router.HitTest(30.0f, 30.0f), button);
        router.DispatchPointer(Pointer(UIPointerAction::DOWN, 153.5f, 128.25f));
        router.DispatchPointer(Pointer(UIPointerAction::UP, 153.5f, 128.25f));
        EXPECT_EQ(clicks, 1);
        router.DispatchPointer(Pointer(UIPointerAction::DOWN, 153.5f, 128.25f));
        router.DispatchPointer(Pointer(UIPointerAction::UP, 30.0f, 30.0f));
        EXPECT_EQ(clicks, 1); // capture still maps release to the button's own layout space
    }

    TEST(UIGeometryTest, SurfaceTransformIsAvailableBeforePaintAndCompatibilityPaintUpdatesIt)
    {
        UIContext context;
        context.SetContentSize(300.0f, 200.0f);
        auto    buttonHolder = std::make_unique<Button>();
        Button *button       = buttonHolder.get();
        button->SetBounds({10.0f, 10.0f, 30.0f, 30.0f});
        context.AddChild(std::move(buttonHolder));
        UI2DTransform surface;
        surface.m00 = 2.0f;
        surface.m11 = 2.0f;
        surface.m02 = 50.0f;
        surface.m12 = 40.0f;
        context.SetSurfaceTransform(surface);
        UIEventRouter router(context);
        EXPECT_EQ(router.HitTest(90.0f, 80.0f), button);
        EXPECT_NE(router.HitTest(20.0f, 20.0f), button);
        EXPECT_TRUE(button->GetGeometry().ContainsSurfacePoint(90.0f, 80.0f));
        UI2DTransform replacement;
        replacement.m02 = 180.0f;
        replacement.m12 = 10.0f;
        UIPaintContext paint;
        context.Paint(paint, replacement);
        ASSERT_EQ(paint.GetDrawData().vertices.size(), 4u);
        EXPECT_FLOAT_EQ(paint.GetDrawData().vertices.front().x, 190.0f);
        EXPECT_FLOAT_EQ(paint.GetDrawData().vertices.front().y, 20.0f);
        EXPECT_FLOAT_EQ(context.GetSurfaceTransform().m02, 180.0f);
        EXPECT_EQ(router.HitTest(200.0f, 30.0f), button);
        EXPECT_NE(router.HitTest(90.0f, 80.0f), button);
        context.Paint(paint);
        ASSERT_EQ(paint.GetDrawData().vertices.size(), 4u);
        EXPECT_FLOAT_EQ(paint.GetDrawData().vertices.front().x, 190.0f);
        EXPECT_FLOAT_EQ(paint.GetDrawData().vertices.front().y, 20.0f);
    }

    TEST(UIGeometryTest, BubbleMapsOriginalSurfaceEventSeparatelyForEveryReceiver)
    {
        UIContext context;
        context.SetContentSize(400.0f, 300.0f);
        UI2DTransform surface;
        surface.m02 = 20.0f;
        surface.m12 = 30.0f;
        context.SetSurfaceTransform(surface);
        auto          parentHolder = std::make_unique<PointerProbe>();
        PointerProbe *parent       = parentHolder.get();
        parent->SetBounds({0.0f, 0.0f, 200.0f, 100.0f});
        UITransform parentTransform;
        parentTransform.translationX = 100.0f;
        parentTransform.translationY = 20.0f;
        parentTransform.scaleX       = 2.0f;
        parentTransform.scaleY       = 2.0f;
        parent->SetTransform(parentTransform);
        auto          childHolder = std::make_unique<PointerProbe>();
        PointerProbe *child       = childHolder.get();
        child->handled            = false;
        child->SetBounds({10.0f, 10.0f, 30.0f, 30.0f});
        UITransform childTransform;
        childTransform.translationX = 10.0f;
        childTransform.translationY = 5.0f;
        child->SetTransform(childTransform);
        parent->AddChild(std::move(childHolder));
        context.AddChild(std::move(parentHolder));
        UIEventRouter  router(context);
        UIPointerEvent event = Pointer(UIPointerAction::DOWN, 180.0f, 100.0f);
        event.pointerId      = 7;
        event.button         = 2;
        event.wheelDelta     = 3.5f;
        EXPECT_EQ(router.DispatchPointer(event), UIEventResult::HANDLED);
        ASSERT_EQ(child->received.size(), 1u);
        ASSERT_EQ(parent->received.size(), 1u);
        ExpectPoint(child->received.back(), 20.0f, 20.0f);
        ExpectPoint(parent->received.back(), 30.0f, 25.0f);
        EXPECT_EQ(parent->received.back().pointerId, 7u);
        EXPECT_EQ(child->received.back().button, 2u);
        EXPECT_FLOAT_EQ(parent->received.back().wheelDelta, 3.5f);
    }

    TEST(UIGeometryTest, HoverAndCaptureRecomputeCoordinatesAfterTransformChanges)
    {
        UIContext context;
        context.SetContentSize(300.0f, 100.0f);
        auto          holder = std::make_unique<PointerProbe>();
        PointerProbe *probe  = holder.get();
        probe->SetBounds({10.0f, 10.0f, 30.0f, 30.0f});
        UITransform transform;
        transform.translationX = 40.0f;
        probe->SetTransform(transform);
        context.AddChild(std::move(holder));
        UIEventRouter router(context);
        router.DispatchPointer(Pointer(UIPointerAction::MOVE, 60.0f, 20.0f));
        ASSERT_EQ(probe->entered.size(), 1u);
        ExpectPoint(probe->entered.back(), 20.0f, 20.0f);
        transform.translationX = 80.0f;
        probe->SetTransform(transform);
        router.DispatchPointer(Pointer(UIPointerAction::MOVE, 60.0f, 20.0f));
        ASSERT_EQ(probe->left.size(), 1u);
        ExpectPoint(probe->left.back(), -20.0f, 20.0f);
        router.DispatchPointer(Pointer(UIPointerAction::DOWN, 100.0f, 20.0f));
        ASSERT_EQ(router.GetCapture(), probe);
        transform.translationX = 120.0f;
        probe->SetTransform(transform);
        router.DispatchPointer(Pointer(UIPointerAction::MOVE, 100.0f, 20.0f));
        ExpectPoint(probe->received.back(), -20.0f, 20.0f);
        UI2DTransform surface;
        surface.m02 = 30.0f;
        context.SetSurfaceTransform(surface);
        router.DispatchPointer(Pointer(UIPointerAction::UP, 170.0f, 20.0f));
        ExpectPoint(probe->received.back(), 20.0f, 20.0f);
        EXPECT_EQ(router.GetCapture(), nullptr);
    }

    TEST(UIGeometryTest, SingularAncestorRejectsHitsAndCapturedDelivery)
    {
        UIContext context;
        context.SetContentSize(100.0f, 100.0f);
        UIElement *parent = context.AddChild(std::make_unique<UIElement>());
        parent->SetBounds({0.0f, 0.0f, 80.0f, 80.0f});
        auto          holder = std::make_unique<PointerProbe>();
        PointerProbe *probe  = holder.get();
        probe->SetBounds({10.0f, 10.0f, 30.0f, 30.0f});
        parent->AddChild(std::move(holder));
        UIEventRouter router(context);
        router.DispatchPointer(Pointer(UIPointerAction::DOWN, 20.0f, 20.0f));
        ASSERT_EQ(router.GetCapture(), probe);
        UITransform singular;
        singular.scaleX = 0.0f;
        parent->SetTransform(singular);
        EXPECT_NE(router.HitTest(20.0f, 20.0f), probe);
        UIPoint layout;
        EXPECT_FALSE(probe->GetGeometry().SurfaceToLayout(20.0f, 20.0f, layout));
        router.DispatchPointer(Pointer(UIPointerAction::MOVE, 20.0f, 20.0f));
        router.DispatchPointer(Pointer(UIPointerAction::UP, 20.0f, 20.0f));
        EXPECT_EQ(probe->received.size(), 1u);
        EXPECT_EQ(router.GetCapture(), nullptr);
    }

    TEST(UIGeometryTest, SingularCaptureCancelsButtonBeforeItsTransformIsRestored)
    {
        UIContext context;
        context.SetContentSize(100.0f, 100.0f);
        UIElement *parent = context.AddChild(std::make_unique<UIElement>());
        parent->SetBounds({0.0f, 0.0f, 80.0f, 80.0f});
        auto    holder = std::make_unique<Button>();
        Button *button = holder.get();
        button->SetBounds({10.0f, 10.0f, 30.0f, 30.0f});
        parent->AddChild(std::move(holder));
        int clicks = 0;
        button->SetOnClick([&clicks]() { ++clicks; });
        UIEventRouter router(context);
        router.DispatchPointer(Pointer(UIPointerAction::DOWN, 20.0f, 20.0f));
        ASSERT_EQ(button->GetState(), Button::State::Pressed);
        UITransform singular;
        singular.scaleX = 0.0f;
        parent->SetTransform(singular);
        router.DispatchPointer(Pointer(UIPointerAction::MOVE, 20.0f, 20.0f));
        EXPECT_EQ(router.GetCapture(), nullptr);
        router.DispatchPointer(Pointer(UIPointerAction::UP, 20.0f, 20.0f));
        parent->SetTransform({});
        EXPECT_EQ(button->GetState(), Button::State::Normal);
        EXPECT_EQ(clicks, 0);
        EXPECT_EQ(router.HitTest(20.0f, 20.0f), button);
    }

    TEST(UIGeometryTest, RotatedClipUsesTheDocumentedConservativeSurfaceAabb)
    {
        UIContext context;
        context.SetContentSize(200.0f, 200.0f);
        UIElement *clipper = context.AddChild(std::make_unique<UIElement>());
        clipper->SetBounds({-10.0f, -10.0f, 10.0f, 10.0f});
        clipper->SetClipsChildren(true);
        UITransform parentTransform;
        parentTransform.translationX = 100.0f;
        parentTransform.translationY = 100.0f;
        parentTransform.rotation     = kQuarterPi;
        clipper->SetTransform(parentTransform);
        auto          holder = std::make_unique<PointerProbe>();
        PointerProbe *probe  = holder.get();
        probe->SetBounds({-50.0f, -50.0f, 50.0f, 50.0f});
        UITransform counterRotation;
        counterRotation.rotation = -kQuarterPi;
        probe->SetTransform(counterRotation);
        clipper->AddChild(std::move(holder));
        UIEventRouter router(context);
        EXPECT_FALSE(clipper->GetGeometry().ContainsSurfacePoint(113.0f, 113.0f));
        EXPECT_EQ(router.HitTest(113.0f, 113.0f), probe);
        EXPECT_NE(router.HitTest(115.0f, 113.0f), probe);
    }

    TEST(UIGeometryTest, ViewportClipStaysInSurfaceSpaceWhenElementsMoveAcrossItsEdge)
    {
        UIContext context;
        context.SetContentSize(100.0f, 100.0f);
        auto          holder = std::make_unique<PointerProbe>();
        PointerProbe *probe  = holder.get();
        probe->SetBounds({120.0f, 10.0f, 140.0f, 30.0f});
        UITransform moveIntoView;
        moveIntoView.translationX = -100.0f;
        probe->SetTransform(moveIntoView);
        context.AddChild(std::move(holder));
        UIEventRouter router(context);
        EXPECT_EQ(router.HitTest(30.0f, 20.0f), probe);
        UI2DTransform surface;
        surface.m02 = 80.0f;
        context.SetSurfaceTransform(surface);
        EXPECT_EQ(router.HitTest(110.0f, 20.0f), nullptr);
    }

    TEST(UIGeometryTest, DirectChildRoutingUsesZOrderAndParentLayoutCoordinates)
    {
        UIElement   parent;
        UITransform parentTransform;
        parentTransform.translationX = 500.0f;
        parent.SetTransform(parentTransform);
        auto          highHolder = std::make_unique<PointerProbe>();
        PointerProbe *high       = highHolder.get();
        high->SetBounds({10.0f, 10.0f, 30.0f, 30.0f});
        UILayoutParams highLayout;
        highLayout.z = 5.0f;
        high->SetLayout(highLayout);
        UITransform highTransform;
        highTransform.translationX = 80.0f;
        highTransform.rotation     = kHalfPi;
        high->SetTransform(highTransform);
        parent.AddChild(std::move(highHolder));
        auto          lowHolder = std::make_unique<PointerProbe>();
        PointerProbe *low       = lowHolder.get();
        low->SetBounds({10.0f, 10.0f, 30.0f, 30.0f});
        UITransform lowTransform;
        lowTransform.translationX = 40.0f;
        low->SetTransform(lowTransform);
        parent.AddChild(std::move(lowHolder));
        UIElement *captured = nullptr;
        EXPECT_EQ(RouteChildPointer(parent.GetChildren(), captured, Pointer(UIPointerAction::DOWN, 60.0f, 20.0f), true), UIEventResult::HANDLED);
        ASSERT_EQ(high->received.size(), 1u);
        EXPECT_TRUE(low->received.empty());
        ExpectPoint(high->received.back(), 20.0f, 20.0f);
        EXPECT_EQ(captured, high);
        highTransform.translationX = 120.0f;
        high->SetTransform(highTransform);
        RouteChildPointer(parent.GetChildren(), captured, Pointer(UIPointerAction::MOVE, 60.0f, 20.0f), true);
        ExpectPoint(high->received.back(), 20.0f, 60.0f);
        RouteChildPointer(parent.GetChildren(), captured, Pointer(UIPointerAction::UP, 60.0f, 20.0f), true);
        EXPECT_EQ(captured, nullptr);
        highTransform.scaleX = 0.0f;
        high->SetTransform(highTransform);
        RouteChildPointer(parent.GetChildren(), captured, Pointer(UIPointerAction::DOWN, 60.0f, 20.0f), true);
        EXPECT_EQ(captured, low);
        ASSERT_EQ(low->received.size(), 1u);
        ExpectPoint(low->received.back(), 20.0f, 20.0f);
    }

    TEST(UIGeometryTest, DirectRoutingRejectsUnrepresentableAccumulatedTransforms)
    {
        const auto verify = [](float scale, float parentPoint) {
            UIElement   parent;
            UITransform transform;
            transform.scaleX = scale;
            transform.scaleY = scale;
            parent.SetTransform(transform);
            auto          holder = std::make_unique<PointerProbe>();
            PointerProbe *child  = holder.get();
            child->SetBounds({0.0f, 0.0f, 10.0f, 10.0f});
            child->SetTransform(transform);
            parent.AddChild(std::move(holder));
            UI2DTransform inverse;
            ASSERT_TRUE(parent.GetTransform().ToMatrix().TryInverse(inverse));
            ASSERT_TRUE(child->GetTransform().ToMatrix().TryInverse(inverse));
            UIElement *captured = nullptr;
            EXPECT_EQ(RouteChildPointer(parent.GetChildren(), captured, Pointer(UIPointerAction::DOWN, parentPoint, parentPoint), true),
                      UIEventResult::UNHANDLED);
            EXPECT_TRUE(child->received.empty());
            EXPECT_EQ(captured, nullptr);
        };
        // Each local inverse is representable, while the composed path is not.
        verify(1.0e-30f, 5.0e-30f);
        verify(1.0e20f, 5.0e20f);
    }

    TEST(UIGeometryTest, DirectRoutingUsesAncestorSurfaceClipWithoutInventingDetachedViewport)
    {
        UIContext context;
        context.SetContentSize(400.0f, 300.0f);
        UI2DTransform surface;
        surface.m00 = 2.0f;
        surface.m11 = 2.0f;
        surface.m02 = 100.0f;
        surface.m12 = 50.0f;
        context.SetSurfaceTransform(surface);
        UIElement *clipper = context.AddChild(std::make_unique<UIElement>());
        clipper->SetBounds({-10.0f, -10.0f, 10.0f, 10.0f});
        clipper->SetClipsChildren(true);
        UITransform clipTransform;
        clipTransform.translationX = 60.0f;
        clipTransform.translationY = 40.0f;
        clipTransform.rotation     = kQuarterPi;
        clipper->SetTransform(clipTransform);
        UIElement  *container = clipper->AddChild(std::make_unique<UIElement>());
        UITransform containerTransform;
        containerTransform.translationX = 10.0f;
        container->SetTransform(containerTransform);
        auto          holder = std::make_unique<PointerProbe>();
        PointerProbe *child  = holder.get();
        child->SetBounds({-50.0f, -50.0f, 50.0f, 50.0f});
        container->AddChild(std::move(holder));
        UIElement *captured = nullptr;
        // In the container's layout space x=8 maps outside the clipper's exact
        // rotated rectangle, but remains inside its surface AABB scissor.
        EXPECT_EQ(RouteChildPointer(container->GetChildren(), captured, Pointer(UIPointerAction::MOVE, 8.0f, 0.0f), true), UIEventResult::HANDLED);
        ASSERT_EQ(child->received.size(), 1u);
        ExpectPoint(child->received.back(), 8.0f, 0.0f);
        EXPECT_EQ(RouteChildPointer(container->GetChildren(), captured, Pointer(UIPointerAction::MOVE, 11.0f, 0.0f), true), UIEventResult::UNHANDLED);
        EXPECT_EQ(child->received.size(), 1u);

        UIElement detached;
        detached.SetBounds({0.0f, 0.0f, 1.0f, 1.0f});
        auto          overflowHolder = std::make_unique<PointerProbe>();
        PointerProbe *overflow       = overflowHolder.get();
        overflow->SetBounds({10.0f, 10.0f, 30.0f, 30.0f});
        detached.AddChild(std::move(overflowHolder));
        EXPECT_EQ(RouteChildPointer(detached.GetChildren(), captured, Pointer(UIPointerAction::MOVE, 20.0f, 20.0f), true), UIEventResult::HANDLED);
        ASSERT_EQ(overflow->received.size(), 1u);
        ExpectPoint(overflow->received.back(), 20.0f, 20.0f);
    }

    TEST(UIGeometryTest, CancelNotifiesAncestorsWithoutDisturbingAnotherPointerCapture)
    {
        UIContext context;
        context.SetContentSize(200.0f, 100.0f);
        auto          parentHolder = std::make_unique<PointerProbe>();
        PointerProbe *parent       = parentHolder.get();
        parent->SetBounds({0.0f, 0.0f, 100.0f, 80.0f});
        auto          firstHolder = std::make_unique<PointerProbe>();
        PointerProbe *first       = firstHolder.get();
        first->SetBounds({10.0f, 10.0f, 30.0f, 30.0f});
        parent->AddChild(std::move(firstHolder));
        auto          secondHolder = std::make_unique<PointerProbe>();
        PointerProbe *second       = secondHolder.get();
        second->SetBounds({50.0f, 10.0f, 70.0f, 30.0f});
        parent->AddChild(std::move(secondHolder));
        context.AddChild(std::move(parentHolder));
        UIEventRouter  router(context);
        UIPointerEvent firstEvent  = Pointer(UIPointerAction::DOWN, 20.0f, 20.0f);
        firstEvent.pointerId       = 11;
        UIPointerEvent secondEvent = Pointer(UIPointerAction::DOWN, 60.0f, 20.0f);
        secondEvent.pointerId      = 22;
        router.DispatchPointer(firstEvent);
        router.DispatchPointer(secondEvent);
        ASSERT_EQ(router.GetCapture(11), first);
        ASSERT_EQ(router.GetCapture(22), second);

        UITransform singular;
        singular.scaleX = 0.0f;
        first->SetTransform(singular);
        firstEvent.action = UIPointerAction::MOVE;
        router.DispatchPointer(firstEvent);
        EXPECT_EQ(first->canceled, (std::vector<uint32_t>{11}));
        EXPECT_EQ(parent->canceled, (std::vector<uint32_t>{11}));
        EXPECT_EQ(first->received.size(), 1u); // cancel has no fabricated layout coordinates
        EXPECT_TRUE(parent->received.empty());
        EXPECT_EQ(router.GetCapture(11), nullptr);
        EXPECT_EQ(router.GetCapture(22), second);

        secondEvent.action = UIPointerAction::MOVE;
        secondEvent.x      = 85.0f;
        secondEvent.y      = 35.0f;
        router.DispatchPointer(secondEvent);
        ExpectPoint(second->received.back(), 85.0f, 35.0f);
        secondEvent.action = UIPointerAction::UP;
        router.DispatchPointer(secondEvent);
        EXPECT_EQ(second->received.size(), 3u);
        EXPECT_TRUE(second->canceled.empty());
        EXPECT_EQ(router.GetCapture(22), nullptr);
    }

    TEST(UIGeometryTest, NestedDirectCaptureForwardsCancelAndRejectsSingularSurface)
    {
        UIContext context;
        context.SetContentSize(100.0f, 100.0f);
        auto               outerHolder  = std::make_unique<ForwardingOverlay>();
        ForwardingOverlay *outer        = outerHolder.get();
        auto               innerHolder  = std::make_unique<ForwardingOverlay>();
        ForwardingOverlay *inner        = innerHolder.get();
        auto               buttonHolder = std::make_unique<Button>();
        Button            *button       = buttonHolder.get();
        inner->AddChild(std::move(buttonHolder));
        outer->AddChild(std::move(innerHolder));
        context.AddChild(std::move(outerHolder));
        outer->Arrange({0.0f, 0.0f, 100.0f, 100.0f});
        int clicks = 0;
        button->SetOnClick([&clicks]() { ++clicks; });
        UIPointerEvent event = Pointer(UIPointerAction::DOWN, 20.0f, 20.0f);
        event.pointerId      = 7;
        ASSERT_EQ(outer->OnPointerEvent(event), UIEventResult::HANDLED);
        ASSERT_EQ(button->GetState(), Button::State::Pressed);
        outer->OnPointerCancel(7);
        EXPECT_EQ(inner->canceled, (std::vector<uint32_t>{7}));
        EXPECT_EQ(button->GetState(), Button::State::Normal);
        event.action = UIPointerAction::UP;
        outer->OnPointerEvent(event);
        EXPECT_EQ(clicks, 0);

        event.action = UIPointerAction::DOWN;
        ASSERT_EQ(outer->OnPointerEvent(event), UIEventResult::HANDLED);
        ASSERT_EQ(button->GetState(), Button::State::Pressed);
        UI2DTransform singularSurface;
        singularSurface.m00 = 0.0f;
        context.SetSurfaceTransform(singularSurface);
        event.action = UIPointerAction::MOVE;
        EXPECT_EQ(outer->OnPointerEvent(event), UIEventResult::UNHANDLED);
        EXPECT_EQ(inner->canceled, (std::vector<uint32_t>{7, 7}));
        EXPECT_EQ(button->GetState(), Button::State::Normal);
        context.SetSurfaceTransform({});
        event.action = UIPointerAction::UP;
        outer->OnPointerEvent(event);
        EXPECT_EQ(button->GetState(), Button::State::Normal);
        EXPECT_EQ(clicks, 0);
    }

} // namespace sky::ui

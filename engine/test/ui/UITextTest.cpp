//
// Created on 2026/09/19.
//

#include <gtest/gtest.h>
#include <ui/IUITextureRegistry.h>
#include <ui/UIPaintContext.h>
#include <ui/text/UIBuiltinFontProvider.h>
#include <ui/text/UIFontAtlas.h>
#include <ui/text/UITextLayout.h>
#include <ui/text/UITextSystem.h>
#include <ui/widgets/Text.h>

#include <limits>
#include <memory>
#include <unordered_map>

namespace sky::ui {

    namespace {

        class FakeTextureRegistry : public IUITextureRegistry {
        public:
            UITextureId RegisterTexture(const UIImageData &image) override
            {
                (void)image;
                registrations++;
                return next++;
            }

            UITextureId RegisterTextureAs(UITextureId id, const UIImageData &image) override
            {
                (void)image;
                registrations++;
                if (id >= next) {
                    next = id + 1;
                }
                return id;
            }

            void UpdateTexture(UITextureId id, const UIImageData &image) override
            {
                (void)id;
                (void)image;
                updates++;
            }

            void ReleaseTexture(UITextureId id) override
            {
                (void)id;
            }

            UITextureId next          = 1;
            int         registrations = 0;
            int         updates       = 0;
        };

        class TallLineFontProvider : public IUIFontProvider {
        public:
            bool GetGlyph(uint32_t codepoint, uint32_t size, UIGlyphBitmap &out) override
            {
                (void)codepoint;
                (void)size;
                out          = {};
                out.width    = 3;
                out.height   = 7;
                out.advance  = 4.0f;
                out.bearingY = 6.0f;
                out.pixels.assign(21, 255);
                return true;
            }

            UIFontMetrics GetFontMetrics(uint32_t size) override
            {
                (void)size;
                return {9.0f, 4.0f, 13.0f};
            }
        };

        class SharedImageRegistry : public IUITextureRegistry {
        public:
            UITextureId RegisterTexture(const UIImageData &image) override
            {
                return RegisterTextureAs(next++, image);
            }
            UITextureId RegisterTextureAs(UITextureId id, const UIImageData &image) override
            {
                ++registrations;
                images.emplace(id, image);
                return id;
            }
            void UpdateTexture(UITextureId id, const UIImageData &image) override
            {
                ++updates;
                if (images.count(id)) {
                    images[id] = image;
                }
            }
            void ReleaseTexture(UITextureId id) override
            {
                ++releases;
                images.erase(id);
            }

            std::unordered_map<UITextureId, UIImageData> images;
            UITextureId                                  next          = 1;
            int                                          registrations = 0;
            int                                          updates       = 0;
            int                                          releases      = 0;
        };

        UIImageData SharedImage(uint8_t value, uint32_t width = 1)
        {
            return {width, 1, std::vector<uint8_t>(static_cast<size_t>(width) * 4, value)};
        }

        UIConstraints WidthBudget(float width)
        {
            return {{}, {width, std::numeric_limits<float>::infinity()}};
        }

        void ExpectGlyphOrigin(const UIDrawData &draw, size_t glyphIndex, float x, float y)
        {
            const size_t vertexIndex = glyphIndex * 4;
            ASSERT_LT(vertexIndex, draw.vertices.size());
            EXPECT_FLOAT_EQ(draw.vertices[vertexIndex].x, x);
            EXPECT_FLOAT_EQ(draw.vertices[vertexIndex].y, y);
        }

    } // namespace

    TEST(UITextTest, MeasureSingleLine)
    {
        FakeTextureRegistry   registry;
        UIBuiltinFontProvider provider;
        UIFontAtlas           atlas(&registry);
        atlas.SetProvider(&provider);

        const UITextExtent extent = UITextLayout::Measure("AB", 10, atlas);
        // advance = floor(0.6 * 10) = 6 per glyph.
        EXPECT_FLOAT_EQ(extent.width, 12.0f);
        EXPECT_FLOAT_EQ(extent.height, 10.0f);
    }

    TEST(UITextTest, MeasureMultiLine)
    {
        FakeTextureRegistry   registry;
        UIBuiltinFontProvider provider;
        UIFontAtlas           atlas(&registry);
        atlas.SetProvider(&provider);

        const UITextExtent extent = UITextLayout::Measure("A\nB", 10, atlas);
        EXPECT_FLOAT_EQ(extent.width, 6.0f);
        EXPECT_FLOAT_EQ(extent.height, 20.0f);
    }

    TEST(UITextTest, AtlasRegistersPageAndReusesGlyph)
    {
        FakeTextureRegistry   registry;
        UIBuiltinFontProvider provider;
        UIFontAtlas           atlas(&registry);
        atlas.SetProvider(&provider);

        const UIGlyphEntry &first = atlas.GetGlyph('A', 10);
        EXPECT_NE(first.textureId, UI_INVALID_TEXTURE);
        EXPECT_EQ(registry.registrations, 1);

        const UIGlyphEntry &again = atlas.GetGlyph('A', 10);
        EXPECT_EQ(again.textureId, first.textureId);
        EXPECT_EQ(registry.registrations, 1);
    }

    TEST(UITextTest, AtlasGrowthKeepsHandles)
    {
        FakeTextureRegistry   registry;
        UIBuiltinFontProvider provider;
        UIFontAtlas           atlas(&registry, 16);
        atlas.SetProvider(&provider);

        // size 10 -> glyph 5x10; a 16x16 page holds three, the fourth needs a new page.
        const UIGlyphEntry &first = atlas.GetGlyph('A', 10);
        atlas.GetGlyph('B', 10);
        atlas.GetGlyph('C', 10);
        atlas.GetGlyph('D', 10);

        EXPECT_GE(atlas.GetPageCount(), 2u);
        EXPECT_GE(registry.registrations, 2);
        // Previously emitted handles stay valid.
        EXPECT_NE(first.textureId, UI_INVALID_TEXTURE);
        EXPECT_EQ(atlas.GetGlyph('A', 10).textureId, first.textureId);
    }

    TEST(UITextTest, AtlasRegistryRemovalAndReplayAreIdempotent)
    {
        FakeTextureRegistry   primary;
        FakeTextureRegistry   secondary;
        FakeTextureRegistry   unknown;
        UIBuiltinFontProvider provider;
        UIFontAtlas           atlas(&primary, 16);
        atlas.SetProvider(&provider);

        const UITextureId firstPage = atlas.GetGlyph('A', 10).textureId;
        atlas.AddRegistry(&secondary);
        atlas.AddRegistry(&secondary);
        atlas.AddRegistry(nullptr);
        EXPECT_EQ(secondary.registrations, 1);
        atlas.GetGlyph('B', 10);
        EXPECT_EQ(secondary.updates, 2);

        atlas.RemoveRegistry(&secondary);
        atlas.RemoveRegistry(&secondary);
        atlas.RemoveRegistry(&unknown);
        atlas.RemoveRegistry(nullptr);
        atlas.GetGlyph('C', 10);
        EXPECT_EQ(secondary.registrations, 1);
        EXPECT_EQ(secondary.updates, 2);
        EXPECT_EQ(primary.updates, 3);

        atlas.AddRegistry(&secondary);
        EXPECT_EQ(secondary.registrations, 2);
        atlas.GetGlyph('D', 10); // A 16x16 page holds three size-10 glyphs.
        EXPECT_EQ(atlas.GetPageCount(), 2u);
        EXPECT_EQ(primary.registrations, 2);
        EXPECT_EQ(secondary.registrations, 3);
        EXPECT_EQ(secondary.updates, 4);
        EXPECT_EQ(atlas.GetGlyph('A', 10).textureId, firstPage);
    }

    TEST(UITextTest, AtlasReattachmentRefreshesRetainedPagesBeforeCachedGlyphUse)
    {
        SharedImageRegistry   primary;
        SharedImageRegistry   secondary;
        UIBuiltinFontProvider provider;
        UIFontAtlas           atlas(&primary, 16);
        atlas.SetProvider(&provider);
        const UITextureId page = atlas.GetGlyph('A', 10).textureId;
        atlas.AddRegistry(&secondary);
        ASSERT_EQ(primary.images.at(page).pixels, secondary.images.at(page).pixels);
        atlas.RemoveRegistry(&secondary);
        atlas.GetGlyph('B', 10);
        ASSERT_EQ(atlas.GetPageCount(), 1u);
        ASSERT_NE(primary.images.at(page).pixels, secondary.images.at(page).pixels);
        atlas.AddRegistry(&secondary);
        EXPECT_EQ(primary.images.at(page).pixels, secondary.images.at(page).pixels);
        const int updates = secondary.updates;
        EXPECT_EQ(atlas.GetGlyph('B', 10).textureId, page);
        EXPECT_EQ(secondary.updates, updates); // Cached glyphs do not upload again.
        EXPECT_EQ(primary.images.at(page).pixels, secondary.images.at(page).pixels);
    }

    TEST(UITextTest, DetachedDestroyedRegistryReceivesNoNewGlyphsOrPages)
    {
        FakeTextureRegistry   primary;
        UIBuiltinFontProvider provider;
        UITextSystem          text(&provider, &primary, 16);
        auto                  secondary = std::make_unique<FakeTextureRegistry>();
        text.AddRegistry(secondary.get());
        text.GetAtlas().GetGlyph('A', 10);

        text.RemoveRegistry(secondary.get());
        secondary.reset();
        // ASan catches both update and new-page replay if detachment is missing.
        text.GetAtlas().GetGlyph('B', 10);
        text.GetAtlas().GetGlyph('C', 10);
        text.GetAtlas().GetGlyph('D', 10);
        EXPECT_EQ(text.GetAtlas().GetPageCount(), 2u);
        EXPECT_EQ(primary.registrations, 2);
        EXPECT_EQ(primary.updates, 4);
        EXPECT_EQ(text.GetRegistry(), &primary);
    }

    TEST(UITextTest, RemovingPrimaryRegistryClearsBorrowedDefaultAndAllowsReattachment)
    {
        FakeTextureRegistry   registry;
        UIBuiltinFontProvider provider;
        UITextSystem          text(&provider, &registry, 16);
        text.GetAtlas().GetGlyph('A', 10);
        text.RemoveRegistry(&registry);
        EXPECT_EQ(text.GetRegistry(), nullptr);
        text.GetAtlas().GetGlyph('B', 10);
        EXPECT_EQ(registry.updates, 1);

        text.AddRegistry(&registry);
        EXPECT_EQ(text.GetRegistry(), &registry);
        EXPECT_EQ(registry.registrations, 2);
        text.GetAtlas().GetGlyph('C', 10);
        EXPECT_EQ(registry.updates, 3);
    }

    TEST(UITextTest, SharedImagesReplayAndPropagateLateRegistrationsAndUpdates)
    {
        SharedImageRegistry primary;
        SharedImageRegistry floating;
        SharedImageRegistry later;
        UITextSystem        text(nullptr, &primary);
        auto               *shared = text.GetSharedRegistry();
        EXPECT_EQ(text.GetRegistry(), &primary);
        const UITextureId icon = shared->RegisterTexture(SharedImage(11));
        EXPECT_EQ(icon, UISharedTextureRegistry::AUTO_FIRST);
        ASSERT_EQ(primary.images.size(), 1u);
        text.AddRegistry(&floating);
        text.AddRegistry(&floating);
        text.AddRegistry(nullptr);
        ASSERT_EQ(floating.images.size(), 1u);
        EXPECT_EQ(floating.registrations, 1);
        EXPECT_EQ(floating.images.at(icon).pixels, primary.images.at(icon).pixels);

        const UITextureId lateIcon = shared->RegisterTexture(SharedImage(22, 2));
        EXPECT_NE(lateIcon, icon);
        EXPECT_EQ(floating.images.at(lateIcon).width, 2u);
        shared->UpdateTexture(icon, SharedImage(33, 3));
        EXPECT_EQ(primary.images.at(icon).pixels, SharedImage(33, 3).pixels);
        EXPECT_EQ(floating.images.at(icon).pixels, primary.images.at(icon).pixels);
        text.AddRegistry(&later);
        EXPECT_EQ(later.images.at(icon).width, 3u);
        EXPECT_EQ(later.images.at(icon).pixels, primary.images.at(icon).pixels);
        EXPECT_EQ(later.images.at(lateIcon).pixels, primary.images.at(lateIcon).pixels);
    }

    TEST(UITextTest, SharedExplicitIdsRemainImmutableAndReensureResetBackends)
    {
        SharedImageRegistry primary;
        SharedImageRegistry floating;
        UITextSystem        text(nullptr, &primary);
        text.AddRegistry(&floating);
        auto *shared = text.GetSharedRegistry();
        EXPECT_EQ(shared->RegisterTextureAs(1, SharedImage(1)), UI_INVALID_TEXTURE);
        EXPECT_EQ(shared->RegisterTextureAs(UISharedTextureRegistry::AUTO_FIRST, SharedImage(1)), UI_INVALID_TEXTURE);
        EXPECT_EQ(shared->RegisterTextureAs(0x40000000u, SharedImage(1)), UI_INVALID_TEXTURE);
        constexpr UITextureId wheel = 0x80000000u;
        EXPECT_EQ(shared->RegisterTextureAs(wheel, SharedImage(44)), wheel);
        EXPECT_EQ(shared->RegisterTextureAs(wheel, SharedImage(55)), wheel);
        EXPECT_EQ(primary.images.at(wheel).pixels, SharedImage(44).pixels);
        primary.images.clear();
        shared->RegisterTextureAs(wheel, SharedImage(55));
        EXPECT_EQ(primary.images.at(wheel).pixels, SharedImage(44).pixels);
        EXPECT_EQ(floating.images.at(wheel).pixels, SharedImage(44).pixels);

        shared->UpdateTexture(wheel, {});
        EXPECT_EQ(primary.images.at(wheel).pixels, SharedImage(44).pixels);
        shared->ReleaseTexture(wheel);
        shared->ReleaseTexture(wheel);
        EXPECT_TRUE(primary.images.empty());
        EXPECT_TRUE(floating.images.empty());
        EXPECT_EQ(primary.releases, 1);
        shared->RegisterTextureAs(wheel, SharedImage(66, 2));
        EXPECT_EQ(primary.images.at(wheel).pixels, SharedImage(66, 2).pixels);
        EXPECT_EQ(floating.images.at(wheel).pixels, SharedImage(66, 2).pixels);
    }

    TEST(UITextTest, SharedExplicitMaximumIdReplaysUpdatesAndReleases)
    {
        SharedImageRegistry   primary;
        SharedImageRegistry   floating;
        UITextSystem          text(nullptr, &primary);
        auto                 *shared = text.GetSharedRegistry();
        constexpr UITextureId last   = std::numeric_limits<UITextureId>::max();
        EXPECT_EQ(shared->RegisterTextureAs(last, SharedImage(99)), last);
        text.AddRegistry(&floating);
        EXPECT_EQ(primary.images.at(last).pixels, SharedImage(99).pixels);
        EXPECT_EQ(floating.images.at(last).pixels, SharedImage(99).pixels);
        shared->UpdateTexture(last, SharedImage(77, 2));
        EXPECT_EQ(floating.images.at(last).pixels, SharedImage(77, 2).pixels);
        shared->ReleaseTexture(last);
        EXPECT_TRUE(primary.images.empty());
        EXPECT_TRUE(floating.images.empty());
    }

    TEST(UITextTest, SharedDetachPreservesLocalResourcesAndNeverCallsDestroyedRegistry)
    {
        SharedImageRegistry   primary;
        SharedImageRegistry   replacement;
        UIBuiltinFontProvider font;
        UITextSystem          text(&font, &primary);
        primary.RegisterTextureAs(1, SharedImage(1)); // Surface-local viewport.
        const UITextureId glyphPage = text.GetAtlas().GetGlyph('A', 10).textureId;
        auto             *shared    = text.GetSharedRegistry();
        const UITextureId icon      = shared->RegisterTexture(SharedImage(11));
        const UITextureId removed   = shared->RegisterTexture(SharedImage(22));
        auto              floating  = std::make_unique<SharedImageRegistry>();
        text.AddRegistry(floating.get());
        EXPECT_EQ(floating->images.count(1), 0u);
        EXPECT_EQ(floating->images.count(glyphPage), 1u);
        text.RemoveRegistry(floating.get());
        text.RemoveRegistry(floating.get());
        text.RemoveRegistry(nullptr);
        const int calls = floating->registrations + floating->updates + floating->releases;
        shared->UpdateTexture(icon, SharedImage(33));
        shared->ReleaseTexture(removed);
        EXPECT_EQ(floating->registrations + floating->updates + floating->releases, calls);
        floating.reset();
        shared->UpdateTexture(icon, SharedImage(44));
        text.GetAtlas().GetGlyph('B', 10);
        shared->ReleaseTexture(1);
        shared->ReleaseTexture(glyphPage);
        EXPECT_EQ(primary.images.count(1), 1u);
        EXPECT_EQ(primary.images.count(glyphPage), 1u);
        text.AddRegistry(&replacement);
        EXPECT_EQ(replacement.images.count(removed), 0u);
        EXPECT_EQ(replacement.images.at(icon).pixels, SharedImage(44).pixels);
        EXPECT_EQ(replacement.images.count(1), 0u);
        EXPECT_EQ(text.GetRegistry(), &primary);
    }

    TEST(UITextTest, SharedCatalogKeepsImagesWithoutSurfacesAndRefreshesReattachedPixels)
    {
        SharedImageRegistry primary;
        SharedImageRegistry otherRegistry;
        UITextSystem        text(nullptr, nullptr);
        UITextSystem        other(nullptr, &otherRegistry);
        auto               *shared = text.GetSharedRegistry();
        const UITextureId   icon   = shared->RegisterTexture(SharedImage(11));
        EXPECT_EQ(text.GetRegistry(), nullptr);
        text.AddRegistry(&primary);
        EXPECT_EQ(text.GetRegistry(), &primary);
        EXPECT_EQ(primary.images.at(icon).pixels, SharedImage(11).pixels);
        text.RemoveRegistry(&primary);
        shared->UpdateTexture(icon, SharedImage(22, 2));
        text.AddRegistry(&primary);
        EXPECT_EQ(primary.images.at(icon).pixels, SharedImage(22, 2).pixels);
        EXPECT_TRUE(otherRegistry.images.empty());
        const UITextureId otherIcon = other.GetSharedRegistry()->RegisterTexture(SharedImage(33));
        EXPECT_EQ(otherIcon, icon); // Independent surface groups have independent catalogs.
        EXPECT_EQ(primary.images.at(icon).pixels, SharedImage(22, 2).pixels);
        EXPECT_EQ(otherRegistry.images.at(otherIcon).pixels, SharedImage(33).pixels);
    }

    TEST(UITextTest, EmitReferencesAtlasHandle)
    {
        FakeTextureRegistry   registry;
        UIBuiltinFontProvider provider;
        UIFontAtlas           atlas(&registry);
        atlas.SetProvider(&provider);

        UIPaintContext context;
        context.Begin({0.0f, 0.0f, 100.0f, 100.0f});
        UITextLayout::Emit(context, "A", 10, 0.0f, 0.0f, 0xFFFFFFFF, atlas);

        ASSERT_FALSE(context.GetDrawData().commands.empty());
        EXPECT_NE(context.GetDrawData().commands[0].textureId, UI_INVALID_TEXTURE);
    }

    TEST(UITextTest, EmitRespectsClip)
    {
        FakeTextureRegistry   registry;
        UIBuiltinFontProvider provider;
        UIFontAtlas           atlas(&registry);
        atlas.SetProvider(&provider);

        UIPaintContext context;
        context.Begin({0.0f, 0.0f, 100.0f, 100.0f});
        context.PushClip({0.0f, 0.0f, 3.0f, 3.0f});
        UITextLayout::Emit(context, "A", 10, 0.0f, 0.0f, 0xFFFFFFFF, atlas);

        ASSERT_FALSE(context.GetDrawData().commands.empty());
        const UIRect &clip = context.GetDrawData().commands[0].clip;
        EXPECT_FLOAT_EQ(clip.right, 3.0f);
        EXPECT_FLOAT_EQ(clip.bottom, 3.0f);
    }

    TEST(UITextTest, WidgetMeasureAndPaint)
    {
        FakeTextureRegistry   registry;
        UIBuiltinFontProvider provider;
        UITextSystem          textSystem(&provider, &registry);

        Text text;
        text.SetTextSystem(&textSystem);
        text.SetContent("AB");
        text.SetFontSize(10);

        float width  = 0.0f;
        float height = 0.0f;
        text.Measure(width, height);
        EXPECT_FLOAT_EQ(width, 12.0f);
        EXPECT_FLOAT_EQ(height, 10.0f);

        text.SetBounds({0.0f, 0.0f, 100.0f, 100.0f});

        UIPaintContext context;
        context.Begin({0.0f, 0.0f, 100.0f, 100.0f});
        text.Paint(context);
        EXPECT_FALSE(context.GetDrawData().commands.empty());
    }

    TEST(UITextTest, WidgetWithoutSystemIsNoOp)
    {
        Text text;
        text.SetContent("AB");

        float width  = 7.0f;
        float height = 7.0f;
        text.Measure(width, height);
        EXPECT_FLOAT_EQ(width, 0.0f);
        EXPECT_FLOAT_EQ(height, 0.0f);

        UIPaintContext context;
        context.Begin({0.0f, 0.0f, 100.0f, 100.0f});
        text.Paint(context);
        EXPECT_TRUE(context.GetDrawData().commands.empty());
    }

    TEST(UITextTest, WrappedMeasureMatchesPaintWithoutChangingElementState)
    {
        FakeTextureRegistry   registry;
        UIBuiltinFontProvider provider;
        UITextSystem          textSystem(&provider, &registry);
        Text                  text;
        text.SetTextSystem(&textSystem);
        text.SetFontSize(10);
        text.SetContent("ABCDE");
        text.SetWrap(true);
        text.Arrange({5.0f, 7.0f, 17.0f, 37.0f});
        text.ClearPaintDirty();

        const UISize narrow = text.MeasureDesired(WidthBudget(12.0f));
        const UISize wide   = text.MeasureDesired(WidthBudget(24.0f));
        EXPECT_FLOAT_EQ(narrow.width, 12.0f);
        EXPECT_FLOAT_EQ(narrow.height, 30.0f);
        EXPECT_FLOAT_EQ(wide.width, 24.0f);
        EXPECT_FLOAT_EQ(wide.height, 20.0f);
        EXPECT_FLOAT_EQ(text.GetBounds().left, 5.0f);
        EXPECT_FLOAT_EQ(text.GetBounds().top, 7.0f);
        EXPECT_FLOAT_EQ(text.GetBounds().right, 17.0f);
        EXPECT_FLOAT_EQ(text.GetBounds().bottom, 37.0f);
        EXPECT_FALSE(text.IsLayoutDirty());
        EXPECT_FALSE(text.IsPaintDirty());

        UIPaintContext context;
        context.Begin({0.0f, 0.0f, 100.0f, 100.0f});
        text.Paint(context);
        const UIDrawData &draw = context.GetDrawData();
        ASSERT_EQ(draw.vertices.size(), 20u);
        ExpectGlyphOrigin(draw, 0, 5.0f, 7.0f);
        ExpectGlyphOrigin(draw, 1, 11.0f, 7.0f);
        ExpectGlyphOrigin(draw, 2, 5.0f, 17.0f);
        ExpectGlyphOrigin(draw, 3, 11.0f, 17.0f);
        ExpectGlyphOrigin(draw, 4, 5.0f, 27.0f);
    }

    TEST(UITextTest, WrappingDeflatesPaddingAndPaintUsesTheSameContentWidth)
    {
        FakeTextureRegistry   registry;
        UIBuiltinFontProvider provider;
        UITextSystem          textSystem(&provider, &registry);
        Text                  text;
        text.SetTextSystem(&textSystem);
        text.SetFontSize(10);
        text.SetContent("ABCD");
        text.SetWrap(true);
        UILayoutParams layout;
        layout.paddingLeft   = 2.0f;
        layout.paddingTop    = 3.0f;
        layout.paddingRight  = 4.0f;
        layout.paddingBottom = 5.0f;
        text.SetLayout(layout);

        const UISize desired = text.MeasureDesired(WidthBudget(18.0f));
        EXPECT_FLOAT_EQ(desired.width, 18.0f);
        EXPECT_FLOAT_EQ(desired.height, 28.0f);
        text.Arrange({10.0f, 20.0f, 28.0f, 48.0f});

        UIPaintContext context;
        context.Begin({0.0f, 0.0f, 100.0f, 100.0f});
        text.Paint(context);
        const UIDrawData &draw = context.GetDrawData();
        ASSERT_EQ(draw.vertices.size(), 16u);
        ExpectGlyphOrigin(draw, 0, 12.0f, 23.0f);
        ExpectGlyphOrigin(draw, 1, 18.0f, 23.0f);
        ExpectGlyphOrigin(draw, 2, 12.0f, 33.0f);
        ExpectGlyphOrigin(draw, 3, 18.0f, 33.0f);
    }

    TEST(UITextTest, FractionalPositionDoesNotWrapAnExactlyFittingFixedWidthLine)
    {
        FakeTextureRegistry   registry;
        UIBuiltinFontProvider provider;
        UITextSystem          textSystem(&provider, &registry);
        Text                  text;
        text.SetTextSystem(&textSystem);
        text.SetFontSize(10);
        text.SetContent("AB");
        text.SetWrap(true);
        UILayoutParams layout;
        layout.width      = {UISizeMode::FIXED, 12.0f};
        layout.offsetLeft = 127.9f;
        text.SetLayout(layout);

        const UISize desired = text.MeasureForLayout(WidthBudget(200.0f));
        EXPECT_FLOAT_EQ(desired.width, 12.0f);
        EXPECT_FLOAT_EQ(desired.height, 10.0f);
        text.Layout({0.0f, 0.0f, 200.0f, 100.0f});
        // Float edge subtraction makes this slightly narrower than 12, but
        // the two advances still describe an exactly fitting logical line.
        EXPECT_LT(text.GetBounds().Width(), 12.0f);
        EXPECT_FLOAT_EQ(text.GetBounds().Height(), 10.0f);

        UIPaintContext context;
        context.Begin({0.0f, 0.0f, 200.0f, 100.0f});
        text.Paint(context);
        const UIDrawData &draw = context.GetDrawData();
        ASSERT_EQ(draw.vertices.size(), 8u);
        ExpectGlyphOrigin(draw, 0, 128.0f, 0.0f);
        ExpectGlyphOrigin(draw, 1, 134.0f, 0.0f);

        // A real reduction in available width still produces a second line.
        EXPECT_FLOAT_EQ(text.MeasureDesired(WidthBudget(11.99f)).height, 20.0f);
    }

    TEST(UITextTest, WrappingPreservesExplicitBlankAndTrailingLines)
    {
        FakeTextureRegistry   registry;
        UIBuiltinFontProvider provider;
        UIFontAtlas           atlas(&registry);
        atlas.SetProvider(&provider);

        const UITextLayoutData layout = UITextLayout::Build("ABC\n\nD\n", 10, atlas, 12.0f, true);
        ASSERT_EQ(layout.lines.size(), 5u);
        EXPECT_EQ(layout.lines[0].text, "AB");
        EXPECT_EQ(layout.lines[1].text, "C");
        EXPECT_EQ(layout.lines[2].text, "");
        EXPECT_EQ(layout.lines[3].text, "D");
        EXPECT_EQ(layout.lines[4].text, "");
        EXPECT_FLOAT_EQ(layout.extent.width, 12.0f);
        EXPECT_FLOAT_EQ(layout.extent.height, 50.0f);

        UIPaintContext context;
        context.Begin({0.0f, 0.0f, 100.0f, 100.0f});
        UITextLayout::Emit(context, layout, 0.0f, 0.0f, 0xFFFFFFFF, atlas);
        const UIDrawData &draw = context.GetDrawData();
        ASSERT_EQ(draw.vertices.size(), 16u);
        ExpectGlyphOrigin(draw, 0, 0.0f, 0.0f);
        ExpectGlyphOrigin(draw, 1, 6.0f, 0.0f);
        ExpectGlyphOrigin(draw, 2, 0.0f, 10.0f);
        ExpectGlyphOrigin(draw, 3, 0.0f, 30.0f);
    }

    TEST(UITextTest, CenterAndRightAlignmentApplyToEveryWrappedAndExplicitLine)
    {
        FakeTextureRegistry   registry;
        UIBuiltinFontProvider provider;
        UITextSystem          textSystem(&provider, &registry);
        Text                  text;
        text.SetTextSystem(&textSystem);
        text.SetFontSize(10);
        text.SetContent("ABC\nD");
        text.SetWrap(true);
        text.Arrange({0.0f, 0.0f, 12.0f, 30.0f});

        text.SetAlign(Text::Align::CENTER);
        UIPaintContext centered;
        centered.Begin({0.0f, 0.0f, 100.0f, 100.0f});
        text.Paint(centered);
        const UIDrawData &centerDraw = centered.GetDrawData();
        ASSERT_EQ(centerDraw.vertices.size(), 16u);
        ExpectGlyphOrigin(centerDraw, 0, 0.0f, 0.0f);
        ExpectGlyphOrigin(centerDraw, 1, 6.0f, 0.0f);
        ExpectGlyphOrigin(centerDraw, 2, 3.0f, 10.0f);
        ExpectGlyphOrigin(centerDraw, 3, 3.0f, 20.0f);

        text.SetAlign(Text::Align::RIGHT);
        UIPaintContext right;
        right.Begin({0.0f, 0.0f, 100.0f, 100.0f});
        text.Paint(right);
        const UIDrawData &rightDraw = right.GetDrawData();
        ASSERT_EQ(rightDraw.vertices.size(), 16u);
        ExpectGlyphOrigin(rightDraw, 0, 0.0f, 0.0f);
        ExpectGlyphOrigin(rightDraw, 1, 6.0f, 0.0f);
        ExpectGlyphOrigin(rightDraw, 2, 6.0f, 10.0f);
        ExpectGlyphOrigin(rightDraw, 3, 6.0f, 20.0f);
    }

    TEST(UITextTest, ZeroWidthAndOversizedGlyphsDoNotCreateExtraEmptyLines)
    {
        FakeTextureRegistry   registry;
        UIBuiltinFontProvider provider;
        UITextSystem          textSystem(&provider, &registry);
        Text                  text;
        text.SetTextSystem(&textSystem);
        text.SetFontSize(10);
        text.SetContent("ABC");
        text.SetWrap(true);

        const UISize zero             = text.MeasureDesired(WidthBudget(0.0f));
        const UISize smallerThanGlyph = text.MeasureDesired(WidthBudget(3.0f));
        EXPECT_FLOAT_EQ(zero.width, 0.0f);
        EXPECT_FLOAT_EQ(zero.height, 30.0f);
        EXPECT_FLOAT_EQ(smallerThanGlyph.width, 3.0f);
        EXPECT_FLOAT_EQ(smallerThanGlyph.height, 30.0f);
        text.Arrange({0.0f, 0.0f, 0.0f, 30.0f});

        UIPaintContext context;
        context.Begin({0.0f, 0.0f, 100.0f, 100.0f});
        text.Paint(context);
        const UIDrawData &draw = context.GetDrawData();
        ASSERT_EQ(draw.vertices.size(), 12u);
        ExpectGlyphOrigin(draw, 0, 0.0f, 0.0f);
        ExpectGlyphOrigin(draw, 1, 0.0f, 10.0f);
        ExpectGlyphOrigin(draw, 2, 0.0f, 20.0f);
    }

    TEST(UITextTest, WrappingDefaultsOffAndLegacyCallsRemainUnbounded)
    {
        FakeTextureRegistry   registry;
        UIBuiltinFontProvider provider;
        UITextSystem          textSystem(&provider, &registry);
        Text                  text;
        text.SetTextSystem(&textSystem);
        text.SetFontSize(10);
        text.SetContent("ABCDE");
        EXPECT_FALSE(text.GetWrap());
        EXPECT_FLOAT_EQ(text.MeasureDesired(WidthBudget(12.0f)).height, 10.0f);

        EXPECT_TRUE(text.SetProperty("text.wrap", UIPropertyValue::Bool(true)));
        EXPECT_FLOAT_EQ(text.MeasureDesired(WidthBudget(12.0f)).height, 30.0f);
        float width  = 0.0f;
        float height = 0.0f;
        text.Measure(width, height);
        EXPECT_FLOAT_EQ(width, 30.0f);
        EXPECT_FLOAT_EQ(height, 10.0f);

        EXPECT_TRUE(text.SetProperty("text.wrap", UIPropertyValue::Bool(false)));
        text.Arrange({0.0f, 0.0f, 12.0f, 10.0f});
        UIPaintContext context;
        context.Begin({0.0f, 0.0f, 100.0f, 100.0f});
        text.Paint(context);
        const UIDrawData &draw = context.GetDrawData();
        ASSERT_EQ(draw.vertices.size(), 20u);
        ExpectGlyphOrigin(draw, 4, 24.0f, 0.0f);

        const UITextExtent legacy = UITextLayout::Measure("ABCDE", 10, textSystem.GetAtlas());
        EXPECT_FLOAT_EQ(legacy.width, 30.0f);
        EXPECT_FLOAT_EQ(legacy.height, 10.0f);
        UIPaintContext legacyContext;
        legacyContext.Begin({0.0f, 0.0f, 100.0f, 100.0f});
        UITextLayout::Emit(legacyContext, "ABCDE", 10, 0.0f, 0.0f, 0xFFFFFFFF, textSystem.GetAtlas());
        ASSERT_EQ(legacyContext.GetDrawData().vertices.size(), 20u);
        ExpectGlyphOrigin(legacyContext.GetDrawData(), 4, 24.0f, 0.0f);
    }

    TEST(UITextTest, WrappedMeasureAndEmitUseProviderLineHeightAndBearing)
    {
        FakeTextureRegistry  registry;
        TallLineFontProvider provider;
        UITextSystem         textSystem(&provider, &registry);
        Text                 text;
        text.SetTextSystem(&textSystem);
        text.SetFontSize(10);
        text.SetContent("ABC");
        text.SetWrap(true);

        const UISize desired = text.MeasureDesired(WidthBudget(8.0f));
        EXPECT_FLOAT_EQ(desired.width, 8.0f);
        EXPECT_FLOAT_EQ(desired.height, 26.0f);
        text.Arrange({0.0f, 0.0f, 8.0f, 26.0f});
        UIPaintContext context;
        context.Begin({0.0f, 0.0f, 100.0f, 100.0f});
        text.Paint(context);
        const UIDrawData &draw = context.GetDrawData();
        ASSERT_EQ(draw.vertices.size(), 12u);
        ExpectGlyphOrigin(draw, 0, 0.0f, 7.0f);
        ExpectGlyphOrigin(draw, 1, 4.0f, 7.0f);
        ExpectGlyphOrigin(draw, 2, 0.0f, 20.0f);
    }

} // namespace sky::ui

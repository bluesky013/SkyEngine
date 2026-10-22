//
// Created on 2026/10/04.
//

#include <core/environment/Singleton.h>
#include <editor/ui/UiDraw.h>
#include <editor/ui/UiTheme.h>
#include <ui/UIElement.h>

#include <algorithm>

namespace sky::editor {

    UiTheme MakeDarkTheme(float scale)
    {
        UiTheme theme;
        theme.name  = "dark";
        scale       = scale < 0.5f ? 0.5f : (scale > 4.0f ? 4.0f : scale);
        theme.scale = scale;

        UiColors &c    = theme.colors;
        c.window       = uidraw::RGB(0x1E, 0x1E, 0x1E);
        c.panel        = uidraw::RGB(0x25, 0x25, 0x26);
        c.header       = uidraw::RGB(0x2D, 0x2D, 0x30);
        c.headerTop    = uidraw::RGB(0x39, 0x39, 0x3E);
        c.section      = uidraw::RGB(0x2A, 0x2A, 0x2D);
        c.sectionTop   = uidraw::RGB(0x31, 0x31, 0x35);
        c.rowEven      = uidraw::RGB(0x25, 0x25, 0x26);
        c.rowOdd       = uidraw::RGB(0x2A, 0x2A, 0x2C);
        c.rowHover     = uidraw::RGB(0x37, 0x37, 0x3D);
        c.rowSelected  = uidraw::RGB(0x09, 0x47, 0x71);
        c.border       = uidraw::RGB(0x3F, 0x3F, 0x46);
        c.borderSoft   = uidraw::RGB(0x33, 0x33, 0x36);
        c.guide        = uidraw::RGB(0x30, 0x30, 0x33);
        c.text         = uidraw::RGB(0xDC, 0xDC, 0xDC);
        c.textMuted    = uidraw::RGB(0x9A, 0x9A, 0x9A);
        c.textDisabled = uidraw::RGB(0x6A, 0x6A, 0x6A);
        c.textOnAccent = uidraw::RGB(0xFF, 0xFF, 0xFF);
        c.accent       = uidraw::RGB(0x0E, 0x63, 0x9C);
        c.accentSoft   = uidraw::RGB(0x14, 0x7A, 0xC2);
        c.accentHover  = uidraw::RGB(0x11, 0x77, 0xBB);
        c.field        = uidraw::RGB(0x1B, 0x1B, 0x1C);
        c.fieldHover   = uidraw::RGB(0x26, 0x26, 0x29);
        c.checkOff     = uidraw::RGB(0x3A, 0x3A, 0x3C);
        c.checkOn      = uidraw::RGB(0x0E, 0x63, 0x9C);
        c.sliderTrack  = uidraw::RGB(0x14, 0x14, 0x15);
        c.sliderFill   = uidraw::RGB(0x0E, 0x63, 0x9C);
        c.sliderHandle = uidraw::RGB(0xD0, 0xD0, 0xD0);
        c.toolbar      = uidraw::RGB(0x32, 0x32, 0x35);
        c.tabActive    = uidraw::RGB(0x1E, 0x1E, 0x1E);
        c.tabInactive  = uidraw::RGB(0x2D, 0x2D, 0x30);
        c.scrollBar    = uidraw::RGB(0x5A, 0x5A, 0x5F);
        c.shadow       = 0x50000000;
        c.error        = uidraw::RGB(0xE0, 0x40, 0x40);
        c.white        = uidraw::RGB(0xFF, 0xFF, 0xFF);
        c.gizmoX         = uidraw::RGB(235, 80, 80);
        c.gizmoY         = uidraw::RGB(100, 220, 110);
        c.gizmoZ         = uidraw::RGB(80, 150, 255);
        c.gizmoHighlight = uidraw::RGB(255, 220, 80);

        theme.metrics.Scale(scale);

        UiFonts   &f  = theme.fonts;
        const auto sf = [scale](uint32_t v) { return static_cast<uint32_t>(v * scale + 0.5f); };
        f.banner      = std::max<uint32_t>(1, sf(f.banner));
        f.title       = std::max<uint32_t>(1, sf(f.title));
        f.section     = std::max<uint32_t>(1, sf(f.section));
        f.label       = std::max<uint32_t>(1, sf(f.label));
        f.value       = std::max<uint32_t>(1, sf(f.value));
        f.small       = std::max<uint32_t>(1, sf(f.small));
        f.tiny        = std::max<uint32_t>(1, sf(f.tiny));
        return theme;
    }

    void UiMetrics::Scale(float scale)
    {
        const auto s = [scale](float &v) { v *= scale; };

        s(panelHeaderHeight);
        s(headerHeight);
        s(sectionHeight);
        s(rowHeight);
        s(tabHeaderHeight);
        s(popupItemHeight);
        s(popupMinWidth);
        s(popupMaxHeight);
        s(panelRadius);
        s(panelGap);

        s(padX);
        s(controlPad);
        s(indentX);
        s(indentSmall);
        s(scrollBarWidth);
        s(itemSpacing);
        s(cellPadding);
        s(hairline);
        s(frameHeight);
        s(iconSize);
        s(gizmoLength);
        s(gizmoWidth);
        s(gizmoHitRadius);

        s(rowRadius);
        s(fieldRadius);
        s(sectionRadius);
        s(buttonRadius);
        s(popupRadius);
        s(checkboxRadius);
        s(swatchRadius);

        s(checkboxSize);
        s(checkboxPad);
        s(sliderTrackHeight);
        s(sliderKnobWidth);
        s(buttonMinWidth);
        s(sliderValueWidth);

        s(listColumnWidth);
        s(listLabelIndent);

        s(dialogMargin);
        s(titleBarHeight);
        s(footerHeight);
        s(toolbarHeight);
        s(menuBarHeight);
        s(statusBarHeight);
        s(sidebarWidth);
        s(listHeaderHeight);
        s(formLabelWidth);
        s(dialogMinWidth);
        s(dialogMinHeight);
        s(fieldWidth);
        s(iconButtonWidth);
        s(controlButtonWidth);
        s(fileBrowserPanelWidth);
        s(fileBrowserPanelHeight);
        s(preferencesPanelWidth);
        s(preferencesPanelHeight);
        s(newWorldPanelWidth);
        s(newWorldPanelHeight);
        s(categoryWidth);
        s(categoryRowHeight);
        s(preferenceRowHeight);
        s(colorRowHeight);

        s(bannerHeight);
        s(hubActionBarHeight);
        s(hubHeaderHeight);
        s(hubRowHeight);
        s(hubButtonWidth);
        s(hubActionWidth);
    }

    // Kept at a stable address so existing UiSkin instances observe updates
    // made by any editor module linked against the static EditorUi library.
    class DefaultUiTheme : public sky::Singleton<DefaultUiTheme> {
        friend class sky::Singleton<DefaultUiTheme>;

    public:
        UiTheme  value    = MakeDarkTheme();
        uint64_t revision = 1;
    };

    const UiTheme &GetDefaultUiTheme()
    {
        return DefaultUiTheme::Get()->value;
    }

    void SetDefaultUiTheme(const UiTheme &theme)
    {
        DefaultUiTheme::Get()->value = theme;
        ++DefaultUiTheme::Get()->revision;
    }

    uint64_t GetDefaultUiThemeRevision()
    {
        return DefaultUiTheme::Get()->revision;
    }

    namespace {
        void InvalidateThemeTree(sky::ui::UIElement &element)
        {
            element.MarkLayoutDirty();
            for (const auto &child : element.GetChildren()) {
                InvalidateThemeTree(*child);
            }
        }
    } // namespace

    bool SynchronizeDefaultUiTheme(sky::ui::UIElement &root, uint64_t &appliedRevision)
    {
        const uint64_t revision = GetDefaultUiThemeRevision();
        if (appliedRevision == revision) {
            return false;
        }
        InvalidateThemeTree(root);
        appliedRevision = revision;
        return true;
    }

    float GetThemeScale()
    {
        return GetDefaultUiTheme().scale;
    }

} // namespace sky::editor

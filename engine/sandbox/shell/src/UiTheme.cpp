//
// Created on 2026/10/04.
//

#include <editor/shell/UiTheme.h>
#include <editor/shell/UiDraw.h>

namespace sky::editor {

    UiTheme MakeDarkTheme()
    {
        UiTheme theme;
        theme.name = "dark";

        UiColors &c = theme.colors;
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
        return theme;
    }

    namespace {

        UiTheme &MutableDefaultTheme()
        {
            static UiTheme theme = MakeDarkTheme();
            return theme;
        }

    } // namespace

    const UiTheme &GetDefaultUiTheme()
    {
        return MutableDefaultTheme();
    }

    void SetDefaultUiTheme(const UiTheme &theme)
    {
        MutableDefaultTheme() = theme;
    }

} // namespace sky::editor

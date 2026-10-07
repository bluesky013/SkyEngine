//
// Created on 2026/10/04.
//

#pragma once

#include <cstdint>
#include <string>

namespace sky::editor {

    // Color palette (ABGR packed, matching the UI vertex format).
    struct UiColors {
        uint32_t window       = 0;
        uint32_t panel        = 0;
        uint32_t header       = 0;
        uint32_t headerTop    = 0;
        uint32_t section      = 0;
        uint32_t sectionTop   = 0;
        uint32_t rowEven      = 0;
        uint32_t rowOdd       = 0;
        uint32_t rowHover     = 0;
        uint32_t rowSelected  = 0;
        uint32_t border       = 0;
        uint32_t borderSoft   = 0;
        uint32_t guide        = 0;
        uint32_t text         = 0;
        uint32_t textMuted    = 0;
        uint32_t textDisabled = 0;
        uint32_t textOnAccent = 0;
        uint32_t accent       = 0;
        uint32_t accentSoft   = 0;
        uint32_t accentHover  = 0;
        uint32_t field        = 0;
        uint32_t fieldHover   = 0;
        uint32_t checkOff     = 0;
        uint32_t checkOn      = 0;
        uint32_t sliderTrack  = 0;
        uint32_t sliderFill   = 0;
        uint32_t sliderHandle = 0;
        uint32_t toolbar      = 0;
        uint32_t tabActive    = 0;
        uint32_t tabInactive  = 0;
        uint32_t scrollBar    = 0;
        uint32_t shadow       = 0;
        uint32_t error        = 0;
        uint32_t white        = 0;
    };

    // Layout metrics; the single source of UI dimensions. `Scale` multiplies every
    // field (the analogue of ImGuiStyle::ScaleAllSizes), so views must read from
    // here instead of hard-coding pixels.
    struct UiMetrics {
        // Panels / chrome.
        float panelHeaderHeight = 27.0f;
        float headerHeight      = 30.0f;
        float sectionHeight     = 26.0f;
        float rowHeight         = 24.0f; // list/form/nav rows
        float tabHeaderHeight   = 26.0f;
        float popupItemHeight   = 20.0f;  // combo/asset popup rows
        float popupMinWidth     = 220.0f; // combo/asset popup minimum width
        float popupMaxHeight    = 260.0f; // combo/asset popup maximum height
        float panelRadius       = 5.0f;   // panel corner radius (Blender-like)
        float panelGap          = 3.0f;   // gap between panels (Blender-like)

        // Spacing / padding.
        float padX           = 10.0f;
        float controlPad     = 8.0f;
        float indentX        = 14.0f;
        float indentSmall    = 16.0f;
        float scrollBarWidth = 8.0f;
        float itemSpacing    = 8.0f;  // gap between adjacent items
        float cellPadding    = 6.0f;  // inner cell padding
        float hairline       = 3.0f;  // small inset / hairline spacing
        float frameHeight    = 26.0f; // control / field / button height
        float iconSize       = 16.0f; // toolbar icon edge length

        // Radii.
        float rowRadius      = 4.0f;
        float fieldRadius    = 4.0f;
        float sectionRadius  = 4.0f;
        float buttonRadius   = 4.0f;
        float popupRadius    = 6.0f;
        float checkboxRadius = 4.0f;
        float swatchRadius   = 4.0f;

        // Controls.
        float checkboxSize      = 16.0f;
        float checkboxPad       = 2.0f;
        float sliderTrackHeight = 6.0f;
        float sliderKnobWidth   = 12.0f;
        float buttonMinWidth    = 88.0f;
        float sliderValueWidth  = 56.0f; // trailing numeric label width next to a slider

        // Panels.
        float listColumnWidth = 150.0f; // config panel subsystem column
        float listLabelIndent = 22.0f;  // config panel name x offset

        // Dialogs.
        float dialogMargin           = 14.0f;
        float titleBarHeight         = 36.0f;
        float footerHeight           = 46.0f;
        float toolbarHeight          = 30.0f;
        float sidebarWidth           = 170.0f; // file browser places
        float listHeaderHeight       = 22.0f;  // list/table header row
        float formLabelWidth         = 78.0f;  // new-world label column
        float dialogMinWidth         = 360.0f;
        float dialogMinHeight        = 260.0f;
        float fieldWidth             = 220.0f; // coordinate / name text field width
        float iconButtonWidth        = 34.0f;  // small square toolbar buttons
        float controlButtonWidth     = 108.0f; // medium toolbar buttons
        float fileBrowserPanelWidth  = 820.0f;
        float fileBrowserPanelHeight = 560.0f;
        float preferencesPanelWidth  = 880.0f;
        float preferencesPanelHeight = 600.0f;
        float newWorldPanelWidth     = 560.0f;
        float newWorldPanelHeight    = 240.0f;
        float categoryWidth          = 200.0f; // preferences category column
        float categoryRowHeight      = 26.0f;
        float preferenceRowHeight    = 30.0f;
        float colorRowHeight         = 64.0f;

        // Project Manager (hub) launcher.
        float bannerHeight       = 64.0f;
        float hubActionBarHeight = 40.0f;
        float hubHeaderHeight    = 28.0f;
        float hubRowHeight       = 32.0f;
        float hubButtonWidth     = 110.0f;
        float hubActionWidth     = 150.0f;

        // Multiplies every metric by scale (the analogue of
        // ImGuiStyle::ScaleAllSizes). Called by MakeDarkTheme.
        void Scale(float scale);
    };

    struct UiFonts {
        uint32_t banner  = 22; // launcher / hero headings
        uint32_t title   = 13;
        uint32_t section = 12;
        uint32_t label   = 12;
        uint32_t value   = 12;
        uint32_t small   = 11;
        uint32_t tiny    = 9;
    };

    struct UiTheme {
        std::string name  = "dark";
        float       scale = 1.0f; // applied DPI/UI scale (metrics+fonts are pre-scaled); used to scale any view constants
        UiColors    colors;
        UiMetrics   metrics;
        UiFonts     fonts;
    };

    // Built-in dark theme (VS Code / Unity / Godot inspired). `scale` multiplies
    // metrics and font sizes so the UI is authored once but drawn crisp at the
    // device pixel ratio (DPI scale = dpi / 96).
    UiTheme MakeDarkTheme(float scale = 1.0f);

    // Process-wide default theme; panels use it unless a custom one is supplied.
    const UiTheme &GetDefaultUiTheme();
    void           SetDefaultUiTheme(const UiTheme &theme);

    // Applied DPI/UI scale of the default theme (the analogue of
    // ImGui::GetFontScale). Views should normally read pre-scaled `metrics`/`fonts`
    // rather than scaling literals themselves; this accessor is for low-level draw
    // utilities that have no theme context.
    float GetThemeScale();

    // Logical font size (device pixels at 100%); scales like UiPx.
    struct UiFont {
        uint32_t value;
        constexpr explicit UiFont(uint32_t v) : value(v)
        {
        }
        operator uint32_t() const
        {
            return static_cast<uint32_t>(value * GetThemeScale() + 0.5f);
        }
    };

} // namespace sky::editor

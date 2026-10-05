//
// Created on 2026/10/04.
//

#pragma once

#include <cstdint>
#include <string>

namespace sky::editor {

    // Color palette (ABGR packed, matching the UI vertex format).
    struct UiColors {
        uint32_t window = 0;
        uint32_t panel = 0;
        uint32_t header = 0;
        uint32_t headerTop = 0;
        uint32_t section = 0;
        uint32_t sectionTop = 0;
        uint32_t rowEven = 0;
        uint32_t rowOdd = 0;
        uint32_t rowHover = 0;
        uint32_t rowSelected = 0;
        uint32_t border = 0;
        uint32_t borderSoft = 0;
        uint32_t guide = 0;
        uint32_t text = 0;
        uint32_t textMuted = 0;
        uint32_t textDisabled = 0;
        uint32_t textOnAccent = 0;
        uint32_t accent = 0;
        uint32_t accentSoft = 0;
        uint32_t accentHover = 0;
        uint32_t field = 0;
        uint32_t fieldHover = 0;
        uint32_t checkOff = 0;
        uint32_t checkOn = 0;
        uint32_t sliderTrack = 0;
        uint32_t sliderFill = 0;
        uint32_t sliderHandle = 0;
        uint32_t toolbar = 0;
        uint32_t tabActive = 0;
        uint32_t tabInactive = 0;
        uint32_t scrollBar = 0;
        uint32_t shadow = 0;
        uint32_t error = 0;
        uint32_t white = 0;
    };

    // Layout metrics; tweak to restyle without touching drawing code.
    struct UiMetrics {
        float panelHeaderHeight = 27.0f;
        float headerHeight = 30.0f;
        float sectionHeight = 26.0f;
        float rowHeight = 24.0f;
        float tabHeaderHeight = 26.0f;
        float popupItemHeight = 20.0f;

        float padX = 10.0f;
        float controlPad = 8.0f;
        float indentX = 14.0f;
        float scrollBarWidth = 8.0f;

        float rowRadius = 4.0f;
        float fieldRadius = 4.0f;
        float sectionRadius = 4.0f;
        float buttonRadius = 4.0f;
        float popupRadius = 6.0f;
        float checkboxRadius = 4.0f;
        float swatchRadius = 4.0f;

        float checkboxSize = 16.0f;
        float sliderTrackHeight = 6.0f;
        float sliderKnobWidth = 12.0f;
    };

    struct UiFonts {
        uint32_t title = 13;
        uint32_t section = 12;
        uint32_t label = 12;
        uint32_t value = 12;
        uint32_t small = 11;
        uint32_t tiny = 9;
    };

    struct UiTheme {
        std::string name = "dark";
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
    void SetDefaultUiTheme(const UiTheme &theme);

} // namespace sky::editor

//
// Created on 2026/10/05.
//

#pragma once

namespace sky::editor {

    // Registers the UI icon builder (SVG -> rasterized RGBA) with the engine's
    // DerivedDataCache so icons are produced once and cached across runs.
    void InstallUiIconBuilder();

} // namespace sky::editor

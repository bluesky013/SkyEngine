//
// Created on 2026/10/04.
//

#pragma once

#include <memory>
#include <string>

namespace sky::ui {
    class UIElement;
    class UITextSystem;
} // namespace sky::ui

namespace sky::editor {

    // Creates a self-contained panel that renders a demo reflected object with
    // the reflection-driven form framework. Used to exercise the framework in a
    // real editor window without a world/selection pipeline.
    std::unique_ptr<sky::ui::UIElement> CreateReflectionDemoPanel(sky::ui::UITextSystem *text,
                                                                  const std::string &title);

} // namespace sky::editor

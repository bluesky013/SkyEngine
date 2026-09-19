## 1. Core support for widgets

- [x] 1.1 Add theme access to `UIPaintContext` (`SetTheme`/`GetTheme`, `const UITheme*`) and seed it in `UIContext::Paint`
- [x] 1.2 Make `UIElement::Paint` push/pop the clip rect when `ClipsChildren()` is set (paint, children, pop)

## 2. Widgets

- [x] 2.1 Add `widgets/Panel.h/.cpp`: clips children and paints the theme background
- [x] 2.2 Add `widgets/Image.h/.cpp`: textured quad over bounds with texture handle, UV, and tint; invalid texture emits nothing
- [x] 2.3 Add `widgets/Button.h/.cpp`: normal/hover/pressed/disabled state, theme colors, disabled ignores input, press+release-inside click callback

## 3. Tests and verification

- [x] 3.1 Add `engine/test/ui/UIWidgetTest.cpp`: panel theme color, theme replacement, panel clips subtree
- [x] 3.2 Add tests: image binds texture handle, image without texture emits nothing, button click inside, release outside does not click, disabled ignores input, pressed state color
- [x] 3.3 Configure, build `UI` and `UICoreTest`, and run the full test suite green (26/26 passing)

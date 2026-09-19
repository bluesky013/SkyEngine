## 1. Layout core

- [x] 1.1 Add `UISizeMode::PERCENT` and `float z` to `UILayoutParams`; resolve percent in `ResolveExtent`
- [x] 1.2 Add `UIElement::GetPaintOrder()` (stable sort by z) and use it in paint and reverse in hit-testing
- [x] 1.3 Add `virtual void ArrangeChildren(const UIRect &content)` and call it from `UIElement::Layout`

## 2. Linear containers

- [x] 2.1 Add `widgets/HBox.{h,cpp}` and `widgets/VBox.{h,cpp}` with `Measure` + `ArrangeChildren` (spacing/padding)
- [x] 2.2 Register `HBox`/`VBox` in `UIElementRegistry::CreateDefault()`
- [x] 2.3 Parse `z` and `"NN%"` sizes in `UIDocumentLoader`

## 3. Tests

- [x] 3.1 Tests: percent width resolves against parent, z paints/hits in order, HBox row placement, VBox measured height
- [x] 3.2 Build and run the suite green

## 1. Property model

- [x] 1.1 Add `data/UIProperty.h` (`UIPropertyValue` typed scalar) and `UIElement::SetProperty(path, value)` handling `name`/`visible`/`enabled`
- [x] 1.2 Override `SetProperty` in `Image` (`visual.texture`, `visual.tint`) and `Button` (`button.interactable`)
- [x] 1.3 Link `3rdParty::rapidjson` into the `UI` target

## 2. Registry and data/binding

- [x] 2.1 Add `data/UIElementRegistry.h/.cpp` (type -> factory) with a default registry seeding `Panel`/`Image`/`Button`
- [x] 2.2 Add `data/IUIDataProvider.h` and `data/UIDataContext.h/.cpp` (named values + change version)
- [x] 2.3 Add `UIBinding` (element/target/source/converter/args) with built-in `remap`/`format`/`boolToVisible`

## 3. Document loader and builder

- [x] 3.1 Add `data/UIDocument.h/.cpp`: `UIDocument` (root/bindings/diagnostics/name index/`ApplyBindings`), `UIDiagnostic`, `UIDocumentLoader::LoadFromString`
- [x] 3.2 Loader applies common + layout props and type props, resolves texture refs via `IUIAssetResolver`, and records diagnostics (never throws)
- [x] 3.3 Add `data/UIBuilder.h/.cpp` producing the same tree and bindings as the loader (parity)

## 4. Tests and verification

- [x] 4.1 Add `engine/test/ui/UIDataTest.cpp`: registry unknown-type error, missing-asset warning, name lookup, bound texture update + skip-when-unchanged, converter remap, builder/loader parity
- [x] 4.2 Configure, build `UI` and `UICoreTest`, and run the full suite green (33/33 passing)

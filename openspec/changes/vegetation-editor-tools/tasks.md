> **Current status — 2026-11-01: blocked-on-prereq.** 等待 vegetation render 的可用预览/绘制闭环。
> Owner: `vegetation-editor-tools`. Prerequisites: `vegetation-aurora-render`.

## 1. Editor extension

- [ ] 1.1 Implement biome authoring (rules, palette, density) in a vegetation editor extension on the aurora sandbox framework (`engine/sandbox` `EditorExtension`, non-Qt)
- [ ] 1.2 Implement density painting/erasing into world-space density maps
- [ ] 1.3 Implement editor preview using the same deterministic placement as runtime
- [ ] 1.4 Implement bake of the configuration into the vegetation asset

## 2. Validation

- [ ] 2.1 Build the sandbox editor and confirm vegetation authoring works end to end

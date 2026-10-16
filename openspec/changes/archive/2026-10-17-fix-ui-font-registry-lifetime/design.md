## Context

UIFontAtlas owns glyph pixels and stable page ids but borrows each IUITextureRegistry. AddRegistry replays existing pages and later GetGlyph calls register/update every attached registry. EditorRenderer owns the concrete registries and therefore must detach them before destroying a surface. No registry token currently owns either participant.

## Decisions

### Pair borrowed registration with idempotent removal

Add UIFontAtlas::RemoveRegistry and forward it from UITextSystem. Removal only erases the borrowed pointer; it never calls the registry, releases textures, or changes atlas glyph/page ownership. Unknown/null removals are harmless. Duplicate AddRegistry remains a no-op; reattaching a removed registry ensures every current page under its existing id and updates its pixels, since a retained backend entry may still contain the pre-detach image.

UITextSystem clears GetRegistry's borrowed default when that same registry is removed. The first later AddRegistry supplies a default again; removing a secondary registry preserves the existing default. This avoids exposing a removed primary registry to consumers such as the color picker.

Registration and glyph preparation remain serialized on the owning UI/render thread. This change does not introduce concurrent mutation or callback reentrancy guarantees.

### Detach at every owner-controlled destruction path

Explicit DestroySurface, the closed-window loop, and global renderer shutdown remove each floating renderer before its Shutdown/destruction. Global shutdown also destroys the text system before shutting down the main renderer. Detachment prevents future CPU calls into freed renderers; it does not wait for submitted GPU work or release the registry's textures. T02 owns that separate resource-lifetime fix, and T07 owns replay of non-font shared textures.

### Verify lifetime behavior rather than pointer presence

Tests verify that removed live registries receive no further calls, that reattachment replays pages once, and that new glyphs/new pages reach remaining live registries after a removed registry is actually destroyed. The last case is run with ASan to cover the reproduced heap-use-after-free.

## Risks

- Callers can still violate a borrowed-pointer contract by destroying a registry without removing it. Document the pairing and integrate all current surface owners.
- Removing a registry intentionally preserves its textures until the owning renderer's GPU-safe shutdown.
- Native floating-window visuals and GPU teardown validation are separate from this CPU lifetime fix.

## Validation

Run UITextTest, affected UI/editor regression suites and consumers, a focused instrumented atlas lifetime reproduction, strict OpenSpec validation, and whitespace checks. Record actual platform limits in tasks.md.

> **Current status — 2026-11-01: planned.** 待补 design/delta；World/renderer 基础接口已交付，不再等待旧 scene-bridge 整体收尾。
> Owner: `audio-world-integration`.

## 1. World attachment

- [ ] 1.1 Add `AudioSystem` to the world in the aurora runtime scene path
- [ ] 1.2 Add `AudioSystem` to the editor document path
- [ ] 1.3 Verify audio is silent-safe when no backend is loaded

## 2. Components

- [ ] 2.1 Register `AudioSourceComponent` / `AudioListenerComponent` with the aurora scene reflection
- [ ] 2.2 Expose audio components in the editor inspector
- [ ] 2.3 Drive the listener from the active camera; update source transforms per tick

## 3. Verification

- [ ] 3.1 Runtime smoke: a source plays from a scene actor
- [ ] 3.2 Validate the change once design/specs are expanded

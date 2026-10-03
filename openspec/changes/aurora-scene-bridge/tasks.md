## 1. Bridge skeleton

- [ ] 1.1 Add `RenderSceneBridge` in `engine/aurora/adaptor` (links `framework` + aurora scene).
- [ ] 1.2 Define the per-frame sync point relative to `World::Tick` and the aurora scene build.
- [ ] 1.3 Map `Actor` identity to an aurora scene entity (deterministic mapping or side table).

## 2. Lifecycle subscriptions

- [ ] 2.1 Subscribe to actor/component attach and detach to create/remove scene entries.
- [ ] 2.2 Subscribe to `TransformComponent` changes to update world transforms.
- [ ] 2.3 Defer scene creation when referenced assets are not yet loaded (reuse `IAssetReadyNotifier`).

## 3. First component coverage

- [ ] 3.1 Static mesh component data.
- [ ] 3.2 Light component data.
- [ ] 3.3 Camera component data.

## 4. One-way and render-free enforcement

- [ ] 4.1 Bridge only reads framework state and writes ECS state (no write-back).
- [ ] 4.2 Bridged component persisted data includes no aurora/render implementation types.

## 5. Verification

- [ ] 5.1 Test: attach/detach creates/removes the ECS entry.
- [ ] 5.2 Test: a parented actor's updated world transform reaches the bridge.
- [ ] 5.3 Build aurora adaptor targets; clang-format/clang-tidy.

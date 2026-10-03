> Status: Frozen. These tasks are not scheduled.

## 1. Dirty flag and lazy resolution

- [ ] 1.1 Add a dirty flag to `TransformComponent`; `EnsureGlobalUpdated()` resolves the parent chain then recomputes `global = parent.global * local` when dirty.
- [ ] 1.2 Make `GetWorldTransform`/`GetWorldMatrix` (and any world-getter) call `EnsureGlobalUpdated()`.
- [ ] 1.3 Change `SetLocal*`/`SetWorld*` to mark self + descendants dirty instead of eager recompute.
- [ ] 1.4 `OnSerialized`/load uses the same lazy path.

## 2. Event delivery

- [ ] 2.1 Ensure the emitter's world transform is current before broadcasting `ITransformEvent::OnTransformChanged`.
- [ ] 2.2 Audit and migrate `engine/render/adaptor` consumers (`CameraComponent`, `LightComponent`, `PrefabComponent`, animation components) to query `GetWorldTransform()` for descendant transforms.
- [ ] 2.3 Add an optional `World` pre-update resolve pass only if profiling requires it (deferred).

## 3. Scale policy

- [ ] 3.1 Decide restrict-with-warning vs correct parent-space composition (D3).
- [ ] 3.2 Implement the chosen behavior in `Transform`/`TransformComponent`; add a diagnostic for the unsupported case.
- [ ] 3.3 Document the policy in the header.

## 4. Tests

- [ ] 4.1 Lazy read returns the current world transform.
- [ ] 4.2 N writes + one read recompute once (no compounding, equals last write).
- [ ] 4.3 Descendants resolve correctly when read after an ancestor write.
- [ ] 4.4 Scale-composition behavior matches the documented policy.
- [ ] 4.5 Core `TransformTest` for the composition rule.

## 5. Verification

- [ ] 5.1 Build framework + render adaptor + tests.
- [ ] 5.2 Run framework and render test suites.
- [ ] 5.3 clang-format/clang-tidy on changed files.

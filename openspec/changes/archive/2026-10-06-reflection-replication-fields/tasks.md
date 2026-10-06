## 1. Metadata key and helper

- [x] 1.1 Add `REPLICATED` to `CommonPropertyKey` (`PropertyCommon.h`).
- [x] 1.2 Add a chainable `SET_REPLICATED()` helper in `SerializationContext.h` modeled on `SET_ASSET_TYPE`.
- [x] 1.3 Add a typed accessor `IsReplicated(const TypeMemberNode&)`.

## 2. Location policy

- [x] 2.1 Flags on the accessor member node; `Data` node serialization-only.
- [x] 2.2 Test `ComponentTest.ReflectionReplicatedFlag` asserts the flag on the component member and absent on the `Data` node.

## 3. Opt-in

- [x] 3.1 Marked `SimpleRotateComponent::Speed` as an example; test component marks `X`/`Y`.
- [x] 3.2 Verified querying replication works through registration.

## 4. Verification

- [x] 4.1 Build framework + tests (FrameworkTest 57/57).
- [x] 4.2 Run framework tests.
- [x] 4.3 clang-format/clang-tidy.

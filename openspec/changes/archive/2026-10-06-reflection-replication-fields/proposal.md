## Why

The framework replication adapter (`actor-replication-source`) must know which component fields participate in network replication. The existing reflection already carries per-member metadata (`TypeMemberNode::properties` with `ASSET_TYPE`, `VISIBLE`, `LABEL_*`), and `network-component-replication` requires components to declare which fields replicate reusing reflection + binary serialization. The minimal addition is a single `REPLICATED` flag on members; editor and scripting continue to use the existing reflection without new flags.

## What Changes

- Add one metadata key `REPLICATED` to the existing per-member metadata channel (`CommonPropertyKey` + `TypeFactory::Property`), plus a chainable helper modeled on `SET_ASSET_TYPE`.
- Keep flags on the component accessor member node; the `Data` struct node stays serialization-only.
- Keep the stable network/type identity (`TypeInfoRT::registeredId` / `Fnv1a32(name)`) separate from metadata.
- Do **not** add speculative flags (script/readonly/range/enum/conditions) — none are required by an existing spec.

## Capabilities

### New Capabilities
- `reflection-replication-fields`: a single per-member `REPLICATED` flag on the reflected component member node, consumed by the replication adapter.

### Modified Capabilities
<!-- runtime-reflection defines member binding; this adds one metadata key. -->

## Impact

- Code: `engine/framework/include/framework/serialization/PropertyCommon.h`, `SerializationContext.h` (registration helper), and component `Reflect` functions that opt in.
- Consumers: `actor-replication-source`.
- No change to the `Data` node or serialized format.

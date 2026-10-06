## Context

`TypeMemberNode` already carries `properties` (`SerializationFactory.h`), written by `TypeFactory::Property`, with `CommonPropertyKey` keys (`VISIBLE`, `LABEL_*`, `ASSET_TYPE`) and the chainable `SET_ASSET_TYPE` helper (`SerializationContext.h`). `network-component-replication` requires declaring replicated fields via reflection.

## Goals / Non-Goals

**Goals:**

- One `REPLICATED` member flag, reused by the replication adapter.

**Non-Goals:**

- Adding other metadata flags (script/readonly/range/enum).
- Replication conditions (not required by any existing spec).
- Changing the `Data` node or serialization.

## Decisions

### D1: Extend `CommonPropertyKey` with `REPLICATED`

Add `REPLICATED` next to the existing keys and a chainable helper:

```
REGISTER_MEMBER(Health, SetHealth, GetHealth) REPLICATED();
```

resolving to `.Property(CommonPropertyKey::REPLICATED, Any(true))` on the accessor member node. Rationale: reuse the existing metadata channel; no new type.

### D2: Flags on the accessor node; stable id separate

Flags live on the component accessor member node (has name + getter/setter). Identity stays `TypeInfoRT::registeredId` / `Fnv1a32(name)`, independent of `properties`.

## Risks / Trade-offs

- [Consumers reading the wrong node] → Keep flags only on the accessor node; the `Data` node is serialization-only.

## Migration Plan

- Additive; existing keys unchanged.
- Rollback: remove the key/helper.

## Open Questions

- None blocking.

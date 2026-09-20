## Context

The runtime reflection subsystem has three layers:

- `engine/core/type` - compile-time type traits (`StaticTypeInfo`, `TypeInfoRT`, `TypeInfoObj<T>`), registered by
  `SerializationContext`.
- `engine/framework/serialization` - the runtime type graph (`TypeNode`/`TypeMemberNode`/`TypeFactory`), value
  erasure (`Any`), container access (`SequenceVisitor`), and the JSON/Binary archives.
- `engine/editor/framework` - Qt widgets built from `TypeNode::members` for property editing.

Registration is manual and fluent (`Register<T>("name").Member<&T::field>("field")`), and archives drive
serialization from the same graph. A review found the defects below; this document records how each is fixed and why.

## Goals / Non-Goals

**Goals:**
- Correct the reviewed defects without changing the public registration API.
- Keep JSON and Binary archives symmetric for the member kinds reflection supports.
- Add regression tests for every fix.

**Non-Goals:**
- Automatic or code-generated registration.
- Expanding `CommonPropertyKey` (min/max/readonly) or an editor widget extension registry.
- Stabilizing `RuntimeTypeId` / cross-platform persistent type ids.
- Archive support for `SequenceVisitor`-typed getter members; components expose those for editing only, and
  serialization uses the direct container members on the underlying data structs.

## Decisions

### D1. `Color` alpha binding
Change `CoreReflection.cpp` `Color` registration from `.Member<&Color::b>("a")` to `.Member<&Color::a>("a")`.
No alternative; the current binding is a typo.

### D2. Editor scalar widget reads the declared type
`PropertyScalar<T>::RefreshValue` currently reads `float*` for every floating type and `uint32_t*` for every
integral type. Read `const T*` from `anyVal.Data()` and format with `if constexpr` on `T`. This preserves the
declared width/format for `double`, `int64_t`/`uint64_t`, and the 8/16-bit types.

### D3. Do not connect unhandled members
Move `connect(widget, ...)` inside the `if (widget != nullptr)` block in `ReflectedObjectWidget`. `std::string`
members (and any future unhandled type) intentionally create no widget; they must not reach `connect(nullptr, ...)`.

### D4. `Any` owns its value
Add a move function to `TypeAllocate<T>` and `TypeInfoRT` (move-construct when available, otherwise fall back to
copy), and fix the operations:
- copy-assign: guard self-assignment, `Destructor()` the old value, then copy;
- move-construct/assign: clear the old value first, then steal the large-type pointer with **no** intermediate
  `CheckMemory()` allocation, or move/copy small types;
- clear `info` after destruction so a destroyed `Any` is empty.

Alternatives considered: making `Any` non-copyable (rejected: used pervasively by value), or a full variant/heap
redesign (rejected: out of scope for a defect fix).

### D5. `SequenceVisitor::GetValueType` null guard
Check `info != nullptr` before reading `info->valueType`; return `Uuid::GetEmpty()` otherwise.

### D6. Binary archive enum and sequence support
- Enum members: in `SaveObject`/`LoadObject`, when the resolved node `isEnum`, recurse with
  `node->info->underlyingTypeId`.
- Sequence members: mirror the JSON layout per member - write/read a `uint32_t` count, then each element via the
  element type from `ContainerInfo::valueType`, using `SequenceVisitor` to iterate/emplace.
- "No silent data loss": replace the empty container skip branch; if a member kind cannot be handled, log/assert
  instead of writing or reading nothing.

### D7. Member function argument validation
Generate a `checkFn` in `MemberFunction` that verifies each argument's registered type against the function's
parameter types (accepting enums as their underlying type, matching `ConstructCheck`). Update
`InvokeMemberFunctionResult` to compare `argsNum` and call `checkFn` before dispatch.

## Risks / Trade-offs

- [Changing `Any` move semantics touches a hot, widely used type] -> keep signatures identical; behavior only
  becomes more correct, and add focused tests for copy/move/self-assign large and small types.
- [Binary format change for sequences/enums could break existing `.bin` assets] -> reflection-driven Binary class
  serialization is currently only exercised by tests and asset payloads that use their own `Save`/`Load`; no
  shipped reflected container/enum binary layout exists to migrate.
- [`checkFn` adds work to invocation] -> negligible; invocation is editor/tooling-facing, not a per-frame path.
- [Sequence round-trip depends on correct `ContainerInfo::valueType`] -> it is already set in `Member<S,G>` for
  sequence members; tests cover vector and list.

## Open Questions

- None blocking. Whether to later unify `SequenceVisitor` getter members with direct container members for
  serialization is deferred to a future change.

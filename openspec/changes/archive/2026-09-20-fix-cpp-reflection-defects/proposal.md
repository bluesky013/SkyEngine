## Why

A review of the runtime C++ reflection subsystem (type graph + manual registration + JSON/Binary archives)
found several correctness defects: one reflected member is wired to the wrong field, the editor property widgets
read values at the wrong width, `Any` leaks/corrupts on copy-move, and the binary archive silently drops enums and
sequence containers. These produce wrong editor values and memory errors, so they must be fixed together with the
review that found them.

## What Changes

- Fix `Color` registration: alpha is currently registered against `Color::b` under the name `"a"`.
- Fix editor reflection widgets:
  - Scalar widget reads the reflected member through its real type instead of always `uint32_t`/`float`.
  - Do not `connect()` a null widget for unhandled member types.
- Fix `Any` ownership: destruct/clear before copy-assign, guard self-assignment, and avoid the extra allocation
  that `Move` leaks for large types (also handle non-copyable small types).
- Guard `SequenceVisitor::GetValueType` against a null container info.
- Make the **Binary** archive handle enum members (by underlying type) and sequence containers symmetrically with
  the JSON archive, instead of silently writing/reading nothing (and asserting on load).
- Validate `MemberFunction` arguments in `MemberFunctionNode::checkFn` instead of always returning true.
- Add regression tests covering every fixed defect.

**Non-goals**: automatic/code-generated reflection registration, expanding `CommonPropertyKey` (min/max/readonly),
an editor property-widget extension registry, and stabilizing the cross-platform type id.

## Capabilities

### New Capabilities
- `runtime-reflection`: the runtime type graph, manual registration, member access, and JSON/Binary serialization
  contract for reflected C++ types.

### Modified Capabilities
<!-- none -->

## Impact

- `engine/core/type` (`Rtti.h`, `TypeInfoObj.h`), `engine/framework/serialization`
  (`Any.*`, `SerializationFactory.h`, `CoreReflection.cpp`, `BinaryArchive.*`, `ArrayVisitor.*`),
  `engine/editor/framework` (`ReflectedObjectWidget.*`).
- `engine/test/framework/SerializationTest.cpp`: new regression tests.
- No public API removed; fixes are behavior corrections.

## Why

Once the Python interpreter runs (see `add-python-runtime`), scripts still cannot touch engine data. The runtime
reflection system already describes every registered C++ type (members, getters/setters, enums, containers,
constructors, member functions), so exposing those types to Python should be generated from that graph instead of
hand-written per-type bindings.

## What Changes

- Add a reflection-driven binding layer to `plugins/python` that walks `SerializationContext`'s `TypeNode` graph
  and creates a Python type per registered type, lazily on first access.
- Expose **members** as Python properties: reads via `getterConstFn`/`getterFn`, writes via `setterFn`;
  const members are read-only.
- Expose **enums** (values + names), **constructors** via `constructList`/`MakeAny`, and **member functions**
  as callable methods that reuse the reflection argument validation.
- Convert values between Python and C++ `Any`: scalars and strings by value; nested reflected types as wrappers;
  sequence containers as mutable list-like objects backed by `SequenceVisitor`.
- Provide a `sky` module surface: lookup a type by name, construct an instance, and inspect registered types.
- Report unknown types/members/functions and argument mismatches as Python exceptions.
- Add tests covering construct, member get/set, const read-only, enum access, sequence mutation, and method call.

**Non-goals** (first iteration): borrowed references to live world objects (bindings own copies), pybind11,
arbitrary STL containers beyond reflected sequences, async/coroutines, and editor UI panels.

## Capabilities

### New Capabilities
- `python-bindings`: reflection-driven exposure of registered C++ types to the embedded Python interpreter.

### Modified Capabilities
<!-- none -->

## Impact

- `plugins/python` (new binding sources and the `sky` module), tests under `engine/test`.
- Depends on `python-runtime` (interpreter) and `runtime-reflection` (`SerializationContext`, `TypeNode`,
  `TypeMemberNode`, `Any`, `SequenceVisitor`).
- No engine API changes; bindings are read-only consumers of the reflection graph.

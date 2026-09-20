## Context

`runtime-reflection` exposes a `TypeNode` per registered type with: `members` (each a `TypeMemberNode` with
`info`, `isConst`, `setterFn`, `getterFn`, `getterConstFn`), `enums`, `constructList`, `functions`, and `base`;
plus `TypeInfoRT` (name, `registeredId`, `underlyingTypeId`, `staticInfo`, `containerInfo`), `Any`, `MakeAny`,
`SequenceVisitor`, and `InvokeMemberFunctionResult`. `add-python-runtime` provides a running interpreter. This
change turns the reflection graph into Python types dynamically, with no per-type binding code.

## Goals / Non-Goals

**Goals:**
- One Python type per registered type, created lazily and cached by registered id.
- Correct member read/write, const enforcement, enums, construction, sequence mutation, and method calls.
- Safe value conversion both ways; failures surface as Python exceptions, never crashes.

**Non-Goals (first iteration):**
- Borrowed references to live C++ objects owned elsewhere (bindings own copies).
- pybind11 or generated C++ binding files.
- Containers other than reflected sequences; content types that reflection does not describe.
- Async, operator overloading beyond sequence protocol, and editor UI.

## Decisions

### D1. Wrapper holds an owning `Any`
A `PySkyObject` stores an owning `Any` (the value) plus the resolved `TypeNode*`. Construction copies a value into
the `Any`; member reads/writes operate on `Any::Data()`. A later change can add a borrowed mode
(`{const TypeInfoRT*, void*}`) for live world objects; the wrapper layout reserves for it. Alternative rejected for
now: always borrowing raw pointers (dangling risk with no ownership tracking).

### D2. Heap type per reflected type with per-member descriptors
Create a heap Python type lazily via `PyType_FromSpec` and cache it in a map keyed by `registeredId`. At creation,
generate one `getset` descriptor per writable member and a read-only descriptor for each `isConst` member; each
descriptor is a `PyGetSetDef` whose closure captures the `TypeNode*` and member key. This gives real Python
attribute semantics and `dir()` introspection. Alternative considered: a single generic `tp_getattro`/`tp_setattro`
(simpler, but no descriptors/introspection) - kept as a fallback if descriptor generation proves unstable.

### D3. Value conversion
`PyObject* -> Any` and `Any -> PyObject*` in one place:
- bool/integrals/floating via `staticInfo` flags and `registeredId`; `std::string` via `PyUnicode`.
- enums via `underlyingTypeId` to `PyLong` (enum values exposed as attributes on the type).
- a registered class member -> a new wrapper owning a copy.
- a sequence member -> a `PySkySequence` view (D5).
- anything unmapped -> `TypeError`.

### D4. Construction
`tp_new` allocates the wrapper without a value; `tp_init` converts arguments to `Any[]` and selects a
`constructList` entry via its `checkFn`, storing the constructed `Any`. Failure raises `TypeError`. A module-level
`make(name, *args)` mirrors this for callers that only have a type name.

### D5. Sequence views are live over the owner's `Any`
`PySkySequence` holds a strong reference to the owning `PySkyObject`, the member key, and a `SequenceVisitor`
constructed over the owner's `Any` member storage. `len`, indexing, `append`, `erase`, and iteration delegate to
the visitor, so mutations are visible on the owning instance's value. Element access converts through D3. This
keeps the "underlying container" semantics without a separate write-back step.

### D6. Member functions
For each entry in `functions`, bind a bound method that converts the Python arguments to `Any[]`, reuses the
reflection argument validation (arity + types), dispatches via the stored `memberFun`, and converts the result
(`void` -> `None`). Argument mismatches raise `TypeError` and never dispatch.

### D7. `sky` module surface
At interpreter init, create a `sky` module exposing `types()` (registered type names), `type(name)`, and
`make(name, *args)`. Type classes are also reachable as attributes after first lookup. Unknown names raise
`KeyError`/`AttributeError` with the missing name.

### D8. Failures are Python exceptions
Every entry point validates pointers and raises an appropriate exception (`TypeError`, `AttributeError`,
`ValueError`) instead of asserting on user input. Python C-API reference counts are handled with RAII-style
helpers to avoid leaks.

## Risks / Trade-offs

- [Lifetime: views/children outlive the owner] -> views keep a strong reference to the owner; child wrappers own
  copies.
- [Reference-count leaks in the C-API glue] -> centralize conversion in RAII helpers and add tests that
  construct/destroy many objects.
- [Descriptor closures require per-member heap objects] -> generate once per type at first access; cache the type.
- [`dir()`/introspection may be incomplete] -> provide `sky.types()` and a documented member list; not a goal to
  fully mirror Python data-model niceties.
- [No borrowed references limits gameplay scripting] -> explicitly deferred; the wrapper layout is designed to add
  it without an API break.

## Open Questions

- Whether `void`-returning functions should return `None` explicitly and whether `out`-parameters are needed.
- Whether enum types should be exposed as Python `IntEnum` subclasses or plain integer constants.
- When borrowed live-object references are needed (separate change).
- Whether sequence element assignment (`__setitem__`) is required in the first iteration.

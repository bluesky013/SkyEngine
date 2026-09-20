## 1. Binding core

- [x] 1.1 Add binding sources under `plugins/python` with a `PySkyObject` wrapper owning an `Any` + resolved `TypeNode*`
- [x] 1.2 Add a per-`registeredId` type cache and lazy heap-type creation via `PyType_FromSpec`
- [x] 1.3 Implement `Any` <-> `PyObject*` conversion for scalars, strings, enums, and reflected classes
- [x] 1.4 Add RAII helpers for Python reference counts around all conversions

## 2. Members

- [x] 2.1 Generate a `getset` descriptor per member (read via getter, write via setter) at type creation
- [x] 2.2 Make `isConst` members read-only (assignment raises and does not modify the field)
- [x] 2.3 Raise `AttributeError` for unknown attributes

## 3. Construction, enums, functions

- [x] 3.1 Implement `tp_new`/`tp_init` using `constructList` + `checkFn`; store the constructed `Any`
- [x] 3.2 Expose enum names/values as type attributes and convert enum values through `underlyingTypeId`
- [x] 3.3 Bind member functions as methods: convert args, reuse reflection argument validation, convert the result (`void` -> `None`)

## 4. Sequence containers

- [x] 4.1 Implement `PySkySequence` as a live view over the owner's `Any` member storage using `SequenceVisitor`
- [x] 4.2 Support length, indexed access, iteration, `append`, and `erase`, with per-element conversion

## 5. `sky` module surface

- [x] 5.1 Create/register a `sky` module exposing `types()`, `type(name)`, and `make(name, *args)` during interpreter init
- [x] 5.2 Raise `KeyError`/`AttributeError` naming the missing type for unknown lookups

## 6. Tests

- [x] 6.1 Construct an instance; read/write a member; verify a const member is read-only
- [x] 6.2 Read and write an enum member and access enum constants
- [x] 6.3 Take a sequence member view, append/erase, and observe size/element changes
- [x] 6.4 Call a member function; verify a mismatched call raises and does not dispatch
- [x] 6.5 Verify unknown type and unknown member raise Python exceptions

## 7. Verification

- [x] 7.1 Build with `-DSKY_BUILD_PYTHON=ON` and run the binding tests
- [x] 7.2 Add a construct/destroy smoke loop to check for reference-count growth
- [x] 7.3 Style checks: ASCII-only, no new long lines, no new warnings

## 1. Registration and core type fixes

- [x] 1.1 Fix `Color` alpha binding in `engine/framework/src/serialization/CoreReflection.cpp` to `&Color::a`
- [x] 1.2 Guard `SequenceVisitor::GetValueType` against null `info` in `engine/framework/src/serialization/ArrayVisitor.cpp`
- [x] 1.3 Add a move function to `TypeAllocate<T>` (`core/type/Rtti.h`) and a `move` pointer to `TypeInfoRT`, wired in `TypeInfoObj<T>`

## 2. `Any` value semantics

- [x] 2.1 Fix `Any::operator=(const Any&)`: guard self-assignment, destruct the old value, then copy; clear `info` after destruction in `Any.cpp`
- [x] 2.2 Fix move-construct and move-assign: destroy existing value first and steal the large-type pointer without an intermediate `CheckMemory()` allocation; move or copy small types (use move fn when available)

## 3. Editor reflection widgets

- [x] 3.1 Make `PropertyScalar<T>::RefreshValue` read the value as `const T*` and format by `T` (`editor/framework/include/editor/framework/ReflectedObjectWidget.h`)
- [x] 3.2 Move `connect(widget, ...)` inside the `widget != nullptr` branch in `engine/editor/framework/src/ReflectedObjectWidget.cpp`

## 4. Member function validation

- [x] 4.1 Generate a real `checkFn` in `TypeFactory::MemberFunction` validating parameter count and types (enum-aware)
- [x] 4.2 Enforce `argsNum` and `checkFn` in `InvokeMemberFunctionResult` before dispatch (`framework/serialization/SerializationContext.h`)

## 5. Binary archive parity

- [x] 5.1 Serialize/deserialize enum members by `underlyingTypeId` in `engine/framework/src/serialization/BinaryArchive.cpp`
- [x] 5.2 Serialize/deserialize sequence members with a `uint32_t` count followed by elements (mirror JSON)
- [x] 5.3 Replace the silent container skip with an explicit report (log/assert) when a member kind cannot be serialized

## 6. Regression tests

- [x] 6.1 Add a test asserting `Color` reflects a writable `a` member distinct from `b`
- [x] 6.2 Add tests for `Any` copy-assign, self-assign, and move of large and small (non-copyable) values
- [x] 6.3 Add a Binary enum round-trip test
- [x] 6.4 Add a Binary sequence round-trip test for vector and list
- [x] 6.5 Add a member function argument-mismatch rejection test

## 7. Verification

- [x] 7.1 Build the engine and the editor target (scalar widget fix)
- [x] 7.2 Build and run `FrameworkTest` (all serialization tests pass)
- [x] 7.3 Run clang-format/clang-tidy on changed files and confirm no new warnings

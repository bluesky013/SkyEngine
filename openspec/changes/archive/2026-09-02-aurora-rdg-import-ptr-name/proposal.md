## Why

RDG 的 `Import` 接口当前接收裸指针 `Image*` / `Buffer*`，导入资源的生命周期归属不清晰——graph 只存一个借用的裸指针，调用方若提前释放会导致悬垂指针。同时资源名 / pass 名用 `std::string`，在 setup 热路径上带来堆分配与字符串拷贝，且身份语义（比较 / hash）依赖逐字符比较。代码库已有现成的侵入式智能指针 `CounterPtr<Image>`（`ImagePtr`）/ `CounterPtr<Buffer>`（`BufferPtr`）与全局 interning 的 `core::Name`，RDG 应统一采用。

## What Changes

- **BREAKING** `RenderGraph::Import` 改为接收 `ImagePtr` / `BufferPtr`（`CounterPtr` 智能指针），不再接收裸指针；graph 在 import payload 中保存该智能指针（持有引用），保证导入资源在 graph 生命周期内不被释放。
- **BREAKING** 所有 RDG 的 `name` 参数与内部存储的名称由 `std::string` 改为 `core::Name`。
- **BREAKING** `RDGContext::GetPassName()` 由 `const char*` 改为 `std::string_view`（由 `Name::GetStr()` 提供）。
- `GraphImportImage::image` / `GraphImportBuffer::buffer` 字段由裸指针改为 `ImagePtr` / `BufferPtr`。
- `RenderGraph::mResolvedImages` / `mResolvedBuffers` 保持为借用的 `Image*` / `Buffer*` 视图（transient 资源仍由池持有，import 资源由 payload 中的智能指针持有）。

## Capabilities

### New Capabilities

无。

### Modified Capabilities

- `aurora-rdg`: `Import` 签名与资源/pass 名称类型（`std::string` → `Name`）发生变化，导入资源生命周期语义变化。
- `aurora-rdg-handles`: `RDGContext::GetPassName()` 返回类型由 `const char*` 改为 `std::string_view`。

## Impact

- 受影响代码：`engine/aurora/rhi/interface/include/aurora/rdg/RenderGraph.h`、`RDGGraph.h`、`RDGContext.h`、`engine/aurora/rhi/interface/src/rdg/RenderGraph.cpp`、`Compile.cpp`、`Execute.cpp`、`engine/aurora/rhi/test/RDGTest.cpp`。
- 新依赖：Aurora RHI 接口层新增对 `core/name/Name.h` 的依赖（接口层已依赖 `core/template/ReferenceObject.h`，`ImagePtr`/`BufferPtr` 即来自该模块）。
- 测试：`RDGTest.cpp` 中 `Import` 调用改为传入 `ImagePtr`/`BufferPtr`；`"t" + std::to_string(i)` 等字符串字面量改为 `Name(...)` 构造。
- 现有调用方：RDG 尚未被 renderer 接入，除测试外无外部调用方受影响。

## Context

Aurora RDG（`aurora/rhi/interface/.../rdg/`）当前：

- `RenderGraph::Import(name, Image *image, ...)` / `Import(name, Buffer *buffer, ...)` 接收裸指针，graph 仅存借用指针（`GraphImportImage.image` 为 `Image*`），生命周期由调用方负责。
- 资源名 / pass 名统一用 `std::string`（`ResourceNode::name`、`PassNode::name`，以及所有 `AddXxxPass` / `CreateXxx` / `Import` 的参数）。
- `RDGContext::GetPassName()` 返回 `const char*`（由 `pass.name.c_str()` 喂入）。

代码库已具备可复用的基础设施：

- `Image` / `Buffer` 继承 `sky::RefObject`（侵入式引用计数），`ImagePtr = CounterPtr<Image>`、`BufferPtr = CounterPtr<Buffer>`（`aurora/rhi/Image.h` / `Buffer.h`）。
- `sky::Name`（`core/name/Name.h`）是全局 interning 的字符串，内部为 `uint32_t` handle，提供 O(1) `operator==` 与 `std::hash`，`GetStr()` 返回 `std::string_view`；已在 render / shader / editor 等处广泛使用。

本 change 是纯接口/内部重构，不改 RDG 的 barrier 推导、culling、transient 池算法。RDG 尚未被 renderer 接入，除 `RDGTest.cpp` 外无外部调用方。

## Goals / Non-Goals

**Goals:**

- `Import` 改用智能指针（`ImagePtr` / `BufferPtr`），graph 持有引用，导入资源在 graph 生命周期内不被释放。
- 所有 RDG 名称由 `std::string` 迁移到 `core::Name`，消除 setup 热路径上的堆分配与逐字符比较。
- 保持后端无关分析逻辑（Compile / Execute）行为不变，仅调整类型与所有权。

**Non-Goals:**

- 不引入新的字符串 interning 机制（复用现有 `core::Name`）。
- 不改 transient 池、barrier 推导、culling 的算法。
- 不引入 `std::shared_ptr` 或其它所有权模型替代 `CounterPtr`。
- 不为 `Name` 增加 `std::string` 隐式转换。

## Decisions

### 1. 智能指针类型选 `CounterPtr`（`ImagePtr`/`BufferPtr`），不用 `std::shared_ptr`

`Image`/`Buffer` 已继承 `sky::RefObject` 并提供 `CounterPtr` 别名。选择 `CounterPtr`：

- 与 aurora RHI 现有所有权模型一致（`Device::CreateImage` 返回裸 `Image*`，调用方用 `CounterPtr<Image>` 包装）。
- 侵入式计数，无 `std::shared_ptr` 控制块双重计数/ABI 问题。

备选：`std::shared_ptr<Image>` —— 与代码库既有 `CounterPtr` 风格冲突，且 `Image` 非 `enable_shared_from_this`，混用易出错，弃用。

### 2. 所有权分层：payload 持有、resolved 表借用

- `GraphImportImage::image` → `ImagePtr`，`GraphImportBuffer::buffer` → `BufferPtr`（持有引用，graph 成员生命周期覆盖）。
- `RenderGraph::mResolvedImages` / `mResolvedBuffers` 保持 `std::vector<Image*>` / `std::vector<Buffer*>` 借用视图：
  - import 资源：填 `mImportImages[i].image.Get()`；
  - transient 资源：填池返回的裸指针（池持有）。

`RDGContext::GetTexture()`/`GetBuffer()` 继续返回裸 `Image*`/`Buffer*`（借用），execute lambda 内引用生命周期由 graph 保证。

### 3. `Import` 参数签名

```cpp
RDGTextureHandle Import(const Name &name, const ImagePtr &image, AccessFlags importAccess = AccessFlagBit::NONE);
RDGBufferHandle  Import(const Name &name, const BufferPtr &buffer, AccessFlags importAccess = AccessFlagBit::NONE);
```

`image`/`buffer` 以 `const&` 传入，payload 内 `import.image = image;` 拷贝一次（+1 ref）。`Name` 按 `const&` 传（与既有 `const std::string &name` 及 `ShaderCompiler` 的 `const Name&` 风格一致）。

### 4. 名称类型 `core::Name`，按 `const Name&` 传递

所有 `const std::string &name` 改为 `const Name &name`；`ResourceNode::name` / `PassNode::name` 改为 `Name`。理由：

- interning 后 O(1) 比较与 hash，无堆分配，身份稳定。
- `Name` 带 `explicit Name(const char*)`，调用方需显式 `Name("backbuffer")` —— 语义更清晰。

备选：`std::string_view`（非 owning，存在悬垂风险）、保留 `std::string`（堆分配 + 慢比较），弃用。

### 5. `RDGContext` 的 pass name

`RDGContext::mPassName` 改为 `Name`，`SetPassName(const Name &)`；`GetPassName()` 返回 `std::string_view`（由 `Name::GetStr()` 提供）。

- 返回 `std::string_view` 而非 `const char*`，因为 `Name::GetStr()` 返回 `std::string_view`（interning 存储稳定，view 生命周期安全）。
- debug 日志经 `Name` 的 `operator<<`（已提供）输出。

## Risks / Trade-offs

- **[`Name` 依赖 `NameDataBase` 单例]** `Name(const char*)` 首次构造会 `NameDataBase::Get()` 惰性初始化；AuroraTest headless 下同样可用（单例惰性建），`FetchOrRegister` 内部有 mutex 保证线程安全。→ 缓解：无需显式初始化；测试内使用字符串字面量构造 `Name` 即可。
- **[`GetStr()` 在 release 有查表开销]** release 下 `GetPassName()` 每次查表。→ 缓解：仅调试/日志路径调用，非每帧热路径。
- **[`Name` 无 `std::string` 构造导致测试改法繁琐]** `"t" + std::to_string(i)` 需改写为 `Name(("t" + std::to_string(i)).c_str())`。→ 缓解：这是有意的类型收紧，测试中封装辅助函数或直接构造。
- **[Import 所有权语义变化]** 从"调用方拥有"变为"graph 持有引用"；若调用方同时以裸指针 `delete`，会破坏。→ 缓解：统一走 `CounterPtr` 管理，文档写明调用方不得在 graph 存活期间 `delete`。

## Migration Plan

1. 改接口头文件（`RenderGraph.h`、`RDGGraph.h`、`RDGContext.h`）。
2. 改实现（`RenderGraph.cpp`、`Compile.cpp`、`Execute.cpp`）。
3. 改 `RDGTest.cpp`：`Import` 传 `ImagePtr`，字符串字面量改 `Name(...)`。
4. 全量编译 + 跑 `AuroraTest`（含 `RDGTest`）验证无回归。
5. archive 本 change（更新 `openspec/specs/aurora-rdg` 与 `aurora-rdg-handles` 的 delta）。

无运行时数据迁移；纯编译期/接口变更，无 rollback 需求（回退即 revert commit）。

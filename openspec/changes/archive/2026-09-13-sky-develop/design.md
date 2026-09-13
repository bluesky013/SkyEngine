## Context

仓库目前用 `_DEBUG`（MSVC 编译器在 Debug 配置下自动定义）作为「调试」信号，`cmake/configuration.cmake` 还注入了 `DEBUG` / `SKY_EDITOR` 等。但 `_DEBUG` 表达的是**编译配置**（Debug vs Release，影响优化、断言、运行时库），而「develop 模式」表达的是**构建意图**（编辑器/开发工具链想要资源名、额外校验、调试 UI，同时保持接近发布的性能）。

`aurora-resource-name` change 里 `SKY_ENABLE_RESOURCE_NAME` 临时用 `_DEBUG` 判定，是一个权宜——它把「要不要资源名」绑到了 Debug 配置，导致 Release 编辑器拿不到资源名、也无法独立开关开发设施。

本 change 引入独立的 `SKY_DEVELOP`，与 `_DEBUG` 解耦，作为所有开发者设施的统一切换点。

## Goals / Non-Goals

**Goals:**

- 引入 `SKY_DEVELOP` 宏（0/1，默认 `0`），表达 develop/开发构建意图。
- CMake 接入：选项开关 + `add_compile_definitions(SKY_DEVELOP=1)`；config 头 `#ifndef` 兜底 `0`。
- `SKY_ENABLE_RESOURCE_NAME` 由 `_DEBUG` 改判 `SKY_DEVELOP`。
- 明确 `_DEBUG`（编译配置）与 `SKY_DEVELOP`（构建意图）分工。

**Non-Goals:**

- 不引入新的 CMake 配置类型（config），只用宏 + 选项表达。
- 不在本 change 新增具体开发设施（资源名在 `aurora-resource-name`，其余后续）。
- 不改变现有 Debug/Release 的优化与断言行为。

## Decisions

### D1: `SKY_DEVELOP` 是「构建意图」而非「编译配置」

```cpp
// core 侧 config 头
#ifndef SKY_DEVELOP
#define SKY_DEVELOP 0
#endif
```

`SKY_DEVELOP=1` 表示「这是给开发者/编辑器用的构建」；`_DEBUG` 仍表示「这是 Debug 编译配置」。二者正交：Debug 可开发可发布；Release 也可开发（编辑器）或发布。

- **理由**：把「开发者要什么设施」与「编译器如何编译」解耦，避免 `_DEBUG` 语义过载。
- **备选**：复用 `SKY_EDITOR` 当 develop —— 被否，`SKY_EDITOR` 语义是「含编辑器功能」，与「开发构建」不完全等价（也可能有非编辑器的开发构建）。

### D2: CMake 接入

`cmake/configuration.cmake` 增加选项（默认跟随 editor/tool 构建开启），开启时 `add_compile_definitions(SKY_DEVELOP=1)`。config 头 `#ifndef` 兜底 `0`，保证即使未走 CMake 也有确定值。

- **理由**：与既有 `SKY_EDITOR` / `SKY_MATH_SIMD` 的 CMake 注入风格一致。
- **备选**：只在 config 头用 `#if defined(_DEBUG) || defined(SKY_EDITOR)` 推导 —— 被否，推导式定义难以被外部显式覆盖，且把意图判定散落到各宏。

### D3: 迁移 `SKY_ENABLE_RESOURCE_NAME` 到 `SKY_DEVELOP`

`aurora-resource-name` 的宏定义改为：

```cpp
#ifndef SKY_ENABLE_RESOURCE_NAME
#  ifdef SKY_DEVELOP
#    define SKY_ENABLE_RESOURCE_NAME 1
#  else
#    define SKY_ENABLE_RESOURCE_NAME 0
#  endif
#endif
```

- **理由**：资源名是典型开发者设施，应挂 `SKY_DEVELOP`；本 change 落地后 `aurora-resource-name` 即可切换，去掉临时 `_DEBUG` 依赖。
- **备选**：`SKY_ENABLE_RESOURCE_NAME` 保持 `_DEBUG` —— 被否，Release 编辑器失去资源名。

## Risks / Trade-offs

- **[默认关导致误用]** 开发者忘记开 `SKY_DEVELOP` → 缓解：editor/tool 构建默认开，文档说明。
- **[多宏矩阵复杂]** `_DEBUG` / `SKY_EDITOR` / `SKY_DEVELOP` 并存 → 缓解：明确各自语义并写进本 spec，不新增无谓组合。

## Migration Plan

- 纯新增宏 + 迁移一处判定（`aurora-resource-name`）。
- 回滚：删除 `SKY_DEVELOP` 定义与 CMake 注入即可。

## Open Questions

- `SKY_DEVELOP` 默认是否在 Release+Editor 构建也开启（倾向是：editor 即 develop）。
- 是否需要一个独立的 CMake 配置（如 `Develop` config）还是仅用宏选项（倾向仅宏）。

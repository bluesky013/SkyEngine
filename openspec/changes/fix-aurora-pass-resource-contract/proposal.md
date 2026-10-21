> 当前状态：本 change 的 Slang→C++ 镜像生成链已由 `refactor-aurora-shared-pass-headers` 替代；以下完成记录保留为阶段历史，不能再作为当前构建规则。

## Why

Status: active（2026-10-20 用户要求起 change 纠偏；保留既有 Global / Pass（View 可选）/ Batch 契约）。

新 Renderer 的最小出图路径将 View/Material/Object 三块固定 UBO 写入 `PreparedRaster` 和生产入口校验，绕开了 Pass 对参数布局的所有权；CMake 也未兑现已归档方案中的自动头文件生成。这是实现偏离既有设计，本 change 恢复原契约，不将最小样例 ABI 升格为新的全局规则。

## What Changes

- 保持 Global set 0、Pass set 1、Batch set 2；View 是具体 Pass 按需消费的数据，不强制所有 shader 声明相机块，也不另占一个固定资源层级。
- Pass 自己维护 Slang 参数定义；CMake 构建依赖生成 shader include 与 C++ 镜像/布局，两侧使用同一份定义源。生成物写 build tree，源声明或依赖变化触发重建，失败不得使用陈旧产物继续构建。
- 复用 `ShaderHeaderTool`、`ShaderCodeGen` 和 Slang 反射，接通 Global 与已有具体 Pass 的生成头消费；不新造 IDL，不用手写 C++ 字段表替代 Slang 布局权威。
- **BREAKING**：纠正生产 Renderer 的固定 View/Material/Object ABI。Pass 提供其布局和数据，`PreparedRaster` 只解析精确资源版本、组装声明，不固定三块 UBO、80/64 字节、Material=set 1 或 set 决定 dynamic 类型。
- 更新生产测试与最小场景，让它们消费生成契约；覆盖有/无 View、不同 Pass 布局、多 View 和在途帧隔离、增量生成与错误布局拒绝。
- 为原 B08 固定 ABI 标注纠偏归属，禁止将修正解释为旧 Global/Pass/Batch 设计被淘汰。保留已有图声明、Compute/Custom 同步和 slot/退休协议。

## Capabilities

### New Capabilities

无。

### Modified Capabilities

- `aurora-resource-binding`：明确三层语义、Pass 的布局所有权及 View 可选性，禁止业务名称推导底层绑定规则。
- `aurora-shader`：落实由 Pass 定义源驱动的 CMake 生成和两侧头文件一致性、构建失败及跨编译契约。
- `aurora-pipeline`：逐 View Renderer 通过 Pass 定义的布局消费 prepare 结果，不再将最小固定 ABI 作为生产管线契约。

## Impact

- `engine/aurora/shader/{tool,src,CMakeLists.txt}`、`core/src/aurora/shader/gen`、`pipeline/{src,include,CMakeLists.txt}`：已有生成器和 Pass 定义/消费链路。
- `core/{include/aurora/renderer,src/renderer/graph,src/renderer/RendererRuntime.cpp}`：固定 ABI 校验、数据来源与构图接入；生产宿主和测试按最终接口迁移。
- 保留 Aurora foundation 归属、现有库和模块入口，不搬目录，不创建 plugin。若实施发现必须改变库依赖/模块职责，先在 design 列出具体调整并遵守根 AGENTS 的确认规则。
- 与 `aurora-compute-custom-passes` 分别跟踪；与 `aurora-renderer-extensions` 划清归属：本 change 负责绑定/生成纠偏，HDR、完整材质资产和更多光照仍留原债务，不重复实施。

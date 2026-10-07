# Change: aurora-cook-schema-layering

> 状态：Debt record（未实现，not scheduled）。记录 review 发现的既有分层债务；未排期，触发条件成熟时再提案。

## Why

`engine/aurora/cook`（builder 目标 `AuroraCook.Static`）当前链接 `Aurora.Adaptor.Static` + `Framework`，并在公开头里 include `aurora/adaptor/assets/*`（`cook/mesh/include/aurora/cook/mesh/MeshProcess.h`、`cook/image/include/aurora/cook/image/ImageAssetWriter.h`）。

根 AGENTS 规定：

- **builder / cook / chef 模块**：离线资产烘焙/转换，**尽量只依赖自身核心模块 + 必要的三方库**；**不依赖** runtime / adaptor / editor。

现状偏离该约束：cook 依赖 adaptor（运行期桥接层）以获取资产 schema（`MeshAssetData` / `ImageAssetData` / `SkinAssetData` 等）。

## What Changes（拟）

- 将 builder 与 adaptor **共享**的资产 schema 从其实现（`aurora/adaptor/assets`）上移到 **engine 侧接口模块**（按消费方归属原则：谁需要、谁定义接口/数据；此处 cook 与 adaptor 均需，故抽为共享的数据/schema 模块），或
- 若判断为一次性例外，则显式记录并放宽该约束（需 review 确认）。

## Tasks（待排期）

- [ ] 1.1 盘点 cook 实际依赖的 adaptor 符号面（schema 类型 / 序列化 trait）
- [ ] 1.2 决定落点：独立 engine schema 模块 vs adaptor 暴露纯数据头
- [ ] 1.3 迁移 schema，去除 `AuroraCook.Static -> Aurora.Adaptor.Static` 链接
- [ ] 1.4 更新 `engine/aurora/AGENTS.md` 分层说明与 CMake 依赖
- [ ] 1.5 构建 + cook 测试回归

## Impact

- `engine/aurora/cook/**`、`engine/aurora/adaptor/**`、可能的 engine schema 模块、CMake

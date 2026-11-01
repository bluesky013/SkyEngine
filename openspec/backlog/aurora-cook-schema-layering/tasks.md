> Status — 2026-11-01: debt（未实现、未排期）。cook/schema 分层债务仍由本记录负责，不列入当前实施清单。
> 触发条件是完成符号面盘点并获批 schema/module 归属方案；保留未完成任务，不补造实现或验收结果。

# Tasks: aurora-cook-schema-layering

（未实现，待排期）

## 1. 分析

- [ ] 1.1 盘点 cook 依赖的 adaptor 符号面（asset schema 类型 / 序列化 trait / component）
- [ ] 1.2 决定 schema 落点：独立 engine schema 模块 vs adaptor 暴露纯数据头

## 2. 实施

- [ ] 2.1 迁移共享 asset schema 出 adaptor
- [ ] 2.2 去除 `AuroraCook.Static -> Aurora.Adaptor.Static` 链接与 `Framework` 依赖
- [ ] 2.3 更新 `engine/aurora/AGENTS.md` 分层说明与 CMake

## 3. 验证

- [ ] 3.1 构建通过（含 cook 目标）
- [ ] 3.2 cook 测试回归

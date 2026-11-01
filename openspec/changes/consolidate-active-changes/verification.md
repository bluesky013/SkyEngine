# 迁移与执行验证 — 2026-11-01

## 当前结果

| 核验 | 结果 |
| --- | --- |
| 分支与基线 | `dev_refactor_rhi`，已 rebase 到 `origin/dev_refactor_rhi` 的 `52fbcb3a` |
| 主 spec | 原 176 个叶子目录 → 32 个领域，当前 1,390 条需求 |
| 当前 change 目录 | 55 → 27（包含明确标记的冻结/债务记录） |
| 本次归档 | 已批准的 24 项，全部保留产物；CLI 使用 `--skip-specs --yes` |
| 未展开草案 | 4 项整体移入 backlog，原 11 个文件及未勾任务完整保留 |
| 全量 strict validation | **59/59 通过：32 spec + 27 change** |
| 相对引用 | 208 条文件链接、12 条 Markdown 锚点，无悬挂 |
| 既有 archive | 1,183 个文件逐 Git blob hash 保留 |
| 本次新 archive | 134 个文件；delta 全部与批准前快照一致，12 份非 delta 文档修复路径引用，其中一处另清理行末空白 |
| 历史 commit manifest | `remove-module-code-leases/commit-scope.md` 原始文件清单完整保留，头部增加定位说明 |
| 改动范围 | 文档/OpenSpec 配置；未修改生产源码或 CMake，无需重编源码 |
| Git 状态 | 无冲突，`git diff --check` 通过；提交与推送按独立确认流程执行 |

## 两阶段证明

先保存本地整理（含独立 async IO 方案），再 rebase，恢复后按新版本重新审计。stash 与执行前完整 OpenSpec 快照仍保留。本次新增同步与原目录迁移分别核验，避免把业务契约更新混为机械搬移。

### 领域迁移

- 最新基线 176 份主 spec 的 **1,327 条既有正文**逐条匹配重新生成的机械迁移结果；只有格式与具体路径按映射归一化。
- 两条同名 console history navigation 增加来源前缀，原正文保留；六份缺 Purpose 的旧 console spec 格式问题随领域合并消除。
- 原 471 个活跃 delta block 完成领域迁移。本维护 change 随范围更新了 5 条 workflow 需求并新增 3 条；其余 466 个继承 block 正文保持。
- 历史 capability ID 通过 [capability-map.json](../../capability-map.json) 定位，不改写旧归档的 ID 或内容。

### 已批准的选择性同步与归档

用户在逐项清单展示后明确答复：**“确认，执行上述归档和草案整理”**。本轮据此执行 [归档清单](archive-review.md)，没有扩大到其余 active/frozen/debt change。

- 24 候选、127 个 delta block：9 项仅保留历史；13 项可安全同步；2 项选择性同步并保留最新正文。
- 精确执行 **65 条新增、2 条过时 ProductGroup 契约移除、3 条正文更新、1 条仅补场景**，主需求 1,327 → 1,390。
- 逐条校验 source/current/result SHA-256；32 个领域头部与来源说明保持，21 条最新 canonical 正文保护通过。最终 32 份主文件与已审查模拟结果逐字节一致。
- 两处 pipeline 需求保留主 spec；Jolt SDK lifetime 保留新版正文，仅追加两项 native-image 场景。活跃 `remove-module-code-leases` 的对应 delta 同步补齐场景，整个 body 现与主 spec 一致。
- 3 条完整更新分别为平台兼容 Jolt ABI/RTTI、Core metadata owner、显式 context 初始化；已对照最新代码核验。
- 24 项在执行前的 artifact graph 均 `isComplete=true`，四项产物 done；这不表示其未勾的专项验收已完成。

## 仍然开放的工作

P02-IOS、ANDROID、NATIVE、COMPUTE、LIGHTING、EDITOR 六项矩阵仍未验收；UIKit input/scene-host 专项由 HarmonyOS 3.10 承接。`remove-module-code-leases` 三个 backend 的 production picking 原生用例仍开放。阶段归档没有勾掉这些任务，也没有把 iPad 导航 demo 扩大解释为全部移动平台验收。

四份无 delta 草案在执行前是全量验证中仅有的 4 个失败项。它们已按批准移入 [backlog](../../backlog/README.md)，保留全文、未实现状态与前置关系；没有编造 delta 或验收结果来消除失败。

本维护 change 自身保留在活跃目录供 review，`workflow` 尚未作为主 spec 发布。独立 common-IO 仍是设计方案。归档授权不包含 commit/push；版本提交与推送按 AGENTS 的独立确认流程执行。

## 可复查命令

```sh
openspec list
openspec validate --all --strict

git diff --check
```

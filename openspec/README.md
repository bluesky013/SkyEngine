# OpenSpec 入口

当前规范以领域为单位维护：`specs/` 保存已接受的契约，`changes/` 保存设计与变更，`changes/archive/` 保存历史。领域是文档归属；核心、runtime、editor、builder、render 的代码边界仍由各条契约明确约束。

2026-11-01 整理基线：`52fbcb3a`。176 份主 spec 合并为 32 个领域，1,327 条既有主需求全部保留。55 个原活跃 change 的 delta 路径与 proposal 能力 ID 同步迁移。仅两个同名的控制台历史导航条目增加来源前缀，其余需求名称保持原样。

经用户确认，24 项已交付/被替代 change 已同步或仅历史归档，4 份未展开草案已移入 [backlog](backlog/README.md)。当前为 **32 个领域、1,390 条主需求、27 个活跃 change 目录**（含明确标记的冻结/债务记录）。未完成的平台验收仍保留在 P02。

- [当前工作、依赖与逐项状态](changes/consolidate-active-changes/design.md)
- [归档预审清单](changes/consolidate-active-changes/archive-review.md)：逐项记录已批准并执行的同步或历史保留方式。
- [迁移与验证记录](changes/consolidate-active-changes/verification.md)
- [旧 capability → 领域映射](capability-map.json)：历史 archive 保留原 ID；查旧名称时用此表定位。

## 当前工作入口

| 方向 | 当前入口与实际边界 |
| --- | --- |
| 渲染与 shader | `refactor-aurora-framegraph-contracts`、`refactor-aurora-target-shader-artifacts`、`refactor-aurora-shader-source-techniques`、`refactor-aurora-slang-usage`；跨平台剩余验收统一看 `aurora-renderer-platform-parity` 的六项 P02 矩阵 |
| 模块与生命周期 | `remove-module-code-leases`；代码进程常驻已经落地，仍保留三个 picking 原生用例的明确验收缺口；实际对象/任务/GPU 退休约束继续成立 |
| 资产与 IO | `add-common-async-io` 仍为设计方案；`refactor-asset-tool-module-config` 的进一步重构单独待排期 |
| 平台交付 | HarmonyOS、Linux、Python SSL/Android Python；基础运行证据与专项恢复、输入、WSI、长期矩阵分开记录 |
| 动画与物理制作 | 骨骼动画、碰撞制作继续按现有 workflow 收口；已有 target/入口不再重复计为待实现；`navigation-terrain-build-tests` 继续推进 terrain+collision→Recast 测试矩阵 |
| 规划池 | 地形/植被渲染与编辑、动画事件、音频桥、网络预测；保持真实前置条件和未实现状态 |

## 领域主规范

| 领域 | 原 capability 数 | 需求数 |
| --- | ---: | ---: |
| [animation](specs/animation/spec.md) | 1 | 43 |
| [asset-cook](specs/asset-cook/spec.md) | 6 | 37 |
| [assets](specs/assets/spec.md) | 6 | 38 |
| [async](specs/async/spec.md) | 2 | 16 |
| [audio](specs/audio/spec.md) | 4 | 21 |
| [aurora-framegraph](specs/aurora-framegraph/spec.md) | 1 | 24 |
| [aurora-pipeline](specs/aurora-pipeline/spec.md) | 2 | 21 |
| [aurora-renderer](specs/aurora-renderer/spec.md) | 5 | 74 |
| [aurora-resources](specs/aurora-resources/spec.md) | 4 | 39 |
| [aurora-rhi](specs/aurora-rhi/spec.md) | 3 | 83 |
| [aurora-shaders](specs/aurora-shaders/spec.md) | 4 | 82 |
| [build](specs/build/spec.md) | 9 | 40 |
| [console](specs/console/spec.md) | 7 | 57 |
| [core](specs/core/spec.md) | 4 | 14 |
| [editor-assets](specs/editor-assets/spec.md) | 5 | 43 |
| [editor-core](specs/editor-core/spec.md) | 7 | 47 |
| [editor-inspector](specs/editor-inspector/spec.md) | 3 | 38 |
| [editor-shell](specs/editor-shell/spec.md) | 15 | 115 |
| [editor-viewport](specs/editor-viewport/spec.md) | 6 | 44 |
| [modules](specs/modules/spec.md) | 4 | 23 |
| [navigation](specs/navigation/spec.md) | 11 | 62 |
| [network](specs/network/spec.md) | 13 | 73 |
| [physics](specs/physics/spec.md) | 15 | 97 |
| [platform](specs/platform/spec.md) | 4 | 33 |
| [project-packaging](specs/project-packaging/spec.md) | 4 | 27 |
| [python](specs/python/spec.md) | 3 | 19 |
| [reflection](specs/reflection/spec.md) | 2 | 8 |
| [terrain](specs/terrain/spec.md) | 5 | 29 |
| [ui](specs/ui/spec.md) | 13 | 100 |
| [vegetation](specs/vegetation/spec.md) | 4 | 18 |
| [visibility](specs/visibility/spec.md) | 1 | 8 |
| [world](specs/world/spec.md) | 3 | 17 |

`workflow` 目前仍是本次维护 change 的待同步领域，不伪造为已归档主规范。

## 维护方式

新增变更优先扩展上述领域，proposal 与 delta 使用同一个 canonical domain ID。确需新增领域时，在 proposal 解释独立边界；不要重新建立历史叶子目录。

旧 delta 不能仅凭任务全勾直接同步。先比较最新主契约，保留进程常驻、真实对象所有权、device 配置及 typed shader 语义；已被替代的内容可以按明确清单仅保留历史。已交付、验收移交、草案和冻结记录分别展示，不把缺失的测试勾掉。

```sh
openspec list
openspec validate --all --strict
```

未展开草案保留在 `backlog/`，不列入 `openspec list` 的当前变更。恢复为活跃 change 前须核对当前代码，补齐 design/delta 和可执行任务；不把草案当作已交付规范。

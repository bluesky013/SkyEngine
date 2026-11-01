# 归档确认清单 — 2026-11-01

状态：**用户已于 2026-11-01 明确确认，24 项归档与 4 份草案整理已执行**。基线 `52fbcb3a`，已 rebase 到最新 origin。

## 范围与方式

- 24 个收尾候选：9 个仅保留历史；13 个可按新基线同步；2 个选择性同步。下面逐项列出方式与未勾任务。
- 选定同步结果：新增 65 条、删除 2 条过时 ProductGroup 契约、更新 3 条正文、仅追加场景 1 条。主需求已从 1,327 → 1,390；仍为 32 个领域。
- 最新进程常驻、实际 compiler/World/SDK/GPU 所有权、typed shader 与 device 一次性 Vulkan 配置契约保留。21 条最新正文设有摘要核验。
- P02 的六项验收和 `remove-module-code-leases` 的三个 picking 原生用例仍未完成；后者不在归档批次。
- `consolidate-active-changes` 本身留在活跃目录，方便 review；本轮不自动 commit/push。

## 24 项逐项状态

任务数为当前文件的已勾/总数。未完成任务不会因归档而改为通过；阶段收尾与旧方案仅历史保留的边界如下。

| Change | 任务 | 处理 | 边界 |
| --- | ---: | --- | --- |
| [remove-unused-legacy-rendergraph](../archive/2026-11-01-remove-unused-legacy-rendergraph/tasks.md) | 8/8 | 仅历史保留，不重放 delta | 已记录该阶段验收；不扩大到其他未验收平台或功能。 |
| [refactor-aurora-pipeline-resource-ownership](../archive/2026-11-01-refactor-aurora-pipeline-resource-ownership/tasks.md) | 43/43 | 选择性同步，保留最新正文 | 补 10 条，保留现有 host assembly 与 compiler snapshot 两条。 |
| [refactor-aurora-framegraph-internals](../archive/2026-11-01-refactor-aurora-framegraph-internals/tasks.md) | 8/8 | 同步经核对的 delta | 已记录该阶段验收；不扩大到其他未验收平台或功能。 |
| [fix-aurora-pass-resource-contract](../archive/2026-11-01-fix-aurora-pass-resource-contract/tasks.md) | 23/25 | 同步经核对的 delta | 2 项未勾：平台验收已移交 P02；另 1 项为确认门禁。 |
| [aurora-compute-custom-passes](../archive/2026-11-01-aurora-compute-custom-passes/tasks.md) | 52/55 | 同步经核对的 delta | 3 项旧 Metal 验收未勾，保留在 P02-COMPUTE。 |
| [aurora-world-editor](../archive/2026-11-01-aurora-world-editor/tasks.md) | 9/9 | 同步经核对的 delta | 已记录该阶段验收；不扩大到其他未验收平台或功能。 |
| [fix-editor-surface-gpu-teardown](../archive/2026-11-01-fix-editor-surface-gpu-teardown/tasks.md) | 8/9 | 仅历史保留，不重放 delta | 旧实现已替代；未勾手动矩阵保留在 P02-EDITOR。 |
| [fix-editor-skipped-frame-fence](../archive/2026-11-01-fix-editor-skipped-frame-fence/tasks.md) | 8/9 | 仅历史保留，不重放 delta | 旧实现已替代；未勾恢复矩阵保留在 P02-EDITOR。 |
| [simplify-asset-product-storage](../archive/2026-11-01-simplify-asset-product-storage/tasks.md) | 24/24 | 选择性同步，保留最新正文 | 补 flat-product；移除 2 条旧事务；更新平台 RTTI；SDK lifetime 仅补 2 个场景。 |
| [refactor-asset-product-transactions](../archive/2026-11-01-refactor-asset-product-transactions/tasks.md) | 5/5 | 仅历史保留，不重放 delta | 中间 ProductGroup 协议已被 flat-product 替代，不重建旧实现。 |
| [refactor-process-owned-reflection](../archive/2026-11-01-refactor-process-owned-reflection/tasks.md) | 16/16 | 仅历史保留，不重放 delta | 旧 delta 会覆盖后续 Animation/进程驻留正文，因此仅保留历史。 |
| [refactor-aurora-shared-pass-headers](../archive/2026-11-01-refactor-aurora-shared-pass-headers/tasks.md) | 8/8 | 仅历史保留，不重放 delta | 已记录该阶段验收；不扩大到其他未验收平台或功能。 |
| [fix-ios-navigation-demo-linkage](../archive/2026-11-01-fix-ios-navigation-demo-linkage/tasks.md) | 7/7 | 同步经核对的 delta | 已记录该阶段验收；不扩大到其他未验收平台或功能。 |
| [fix-android-application-lifecycle](../archive/2026-11-01-fix-android-application-lifecycle/tasks.md) | 18/18 | 同步经核对的 delta | 已记录该阶段验收；不扩大到其他未验收平台或功能。 |
| [verify-ios-world-packaging](../archive/2026-11-01-verify-ios-world-packaging/tasks.md) | 20/20 | 同步经核对的 delta | 已记录该阶段验收；不扩大到其他未验收平台或功能。 |
| [enforce-cmake-source-ownership](../archive/2026-11-01-enforce-cmake-source-ownership/tasks.md) | 7/7 | 同步经核对的 delta | 已记录该阶段验收；不扩大到其他未验收平台或功能。 |
| [fix-animation-sampling-validation](../archive/2026-11-01-fix-animation-sampling-validation/tasks.md) | 5/5 | 仅历史保留，不重放 delta | 已记录该阶段验收；不扩大到其他未验收平台或功能。 |
| [fix-sandbox-project-creation](../archive/2026-11-01-fix-sandbox-project-creation/tasks.md) | 10/10 | 同步经核对的 delta | 已记录该阶段验收；不扩大到其他未验收平台或功能。 |
| [add-ios-platform](../archive/2026-11-01-add-ios-platform/tasks.md) | 0/10 | 仅历史保留，不重放 delta | 0/10 为旧规划；输入/scene-host 移交 Harmony 3.10，GPU 退休移交 P02-IOS。 |
| [add-ios-uikit-backend](../archive/2026-11-01-add-ios-uikit-backend/tasks.md) | 8/13 | 仅历史保留，不重放 delta | 8/13；剩余输入/scene-host 与 GPU 验收同上，不标已完成。 |
| [refactor-vulkan-device-configuration](../archive/2026-11-01-refactor-vulkan-device-configuration/tasks.md) | 6/6 | 同步经核对的 delta | 已记录该阶段验收；不扩大到其他未验收平台或功能。 |
| [refactor-renderer-runtime-flow](../archive/2026-11-01-refactor-renderer-runtime-flow/tasks.md) | 6/6 | 同步经核对的 delta | 已记录该阶段验收；不扩大到其他未验收平台或功能。 |
| [refactor-aurora-runtime-service-lifecycle](../archive/2026-11-01-refactor-aurora-runtime-service-lifecycle/tasks.md) | 5/5 | 同步经核对的 delta | 已记录该阶段验收；不扩大到其他未验收平台或功能。 |
| [add-party-softbody-project](../archive/2026-11-01-add-party-softbody-project/tasks.md) | 31/31 | 同步经核对的 delta | 已记录该阶段验收；不扩大到其他未验收平台或功能。 |

## 同步逐条清单

以下是主 spec 的精确操作；未列出的正文保留。`preserve` 两条也显式列出，防止旧 ADDED 覆盖当前语义。完整正文在相应候选 delta 与当前领域主 spec 中可对照。

| Change | 领域 / 需求 | 操作 |
| --- | --- | --- |
| `refactor-aurora-pipeline-resource-ownership` | `aurora-renderer` / Scene techniques match RenderQueue Tags | 新增 |
| `refactor-aurora-pipeline-resource-ownership` | `aurora-renderer` / Fullscreen and compute select techniques explicitly | 新增 |
| `refactor-aurora-pipeline-resource-ownership` | `aurora-renderer` / View shader data belongs to Pass | 新增 |
| `refactor-aurora-pipeline-resource-ownership` | `aurora-renderer` / Material and Batch have separate providers | 新增 |
| `refactor-aurora-pipeline-resource-ownership` | `aurora-renderer` / Custom pipelines consume foundation binding mechanisms | 新增 |
| `refactor-aurora-pipeline-resource-ownership` | `aurora-renderer` / Host explicitly assembles pipeline owners | 保留主 spec，跳过旧 delta |
| `refactor-aurora-pipeline-resource-ownership` | `aurora-shaders` / Owner-scoped shared declarations | 新增 |
| `refactor-aurora-pipeline-resource-ownership` | `aurora-shaders` / Logical bindings remain independent of native argument slots | 新增 |
| `refactor-aurora-pipeline-resource-ownership` | `aurora-shaders` / Unified module resource registration | 新增 |
| `refactor-aurora-pipeline-resource-ownership` | `aurora-shaders` / Uniform module shader roots and unique paths | 新增 |
| `refactor-aurora-pipeline-resource-ownership` | `aurora-shaders` / Runtime and cook share owner resource descriptions | 新增 |
| `refactor-aurora-pipeline-resource-ownership` | `aurora-shaders` / Compilation retains its source snapshot | 保留主 spec，跳过旧 delta |
| `refactor-aurora-framegraph-internals` | `aurora-framegraph` / FrameGraph internal stages preserve declaration and lifetime semantics | 新增 |
| `fix-aurora-pass-resource-contract` | `aurora-pipeline` / PreparedRaster 消费 Pass 契约 | 新增 |
| `fix-aurora-pass-resource-contract` | `aurora-shaders` / Pass 绑定按实际 shader 子集装配 | 新增 |
| `fix-aurora-pass-resource-contract` | `aurora-shaders` / Renderer 保持四层资源语义 | 新增 |
| `fix-aurora-pass-resource-contract` | `aurora-shaders` / 布局共享与帧数据隔离 | 新增 |
| `fix-aurora-pass-resource-contract` | `aurora-shaders` / 完整 Pass 资源声明 | 新增 |
| `aurora-compute-custom-passes` | `aurora-framegraph` / Compute/Custom pass 的声明访问 | 新增 |
| `aurora-compute-custom-passes` | `aurora-framegraph` / Compute/Custom 的后端编码 | 新增 |
| `aurora-compute-custom-passes` | `aurora-framegraph` / Raster pass execute 由冻结 draw 驱动 | 正文相同，不改 |
| `aurora-world-editor` | `editor-viewport` / Editable World reaches the renderer through one producer | 新增 |
| `aurora-world-editor` | `editor-viewport` / Scene editing preserves logical data and ownership | 新增 |
| `aurora-world-editor` | `editor-viewport` / Inspector bindings remain valid across document changes | 新增 |
| `aurora-world-editor` | `editor-viewport` / Scene hierarchy and component inspection remain separate | 新增 |
| `aurora-world-editor` | `editor-viewport` / Targeted editor module builds refresh runtime dependencies | 新增 |
| `simplify-asset-product-storage` | `assets` / Generic product transactions publish complete generations | 移除旧条目 |
| `simplify-asset-product-storage` | `assets` / Product snapshots retain their selected generation | 移除旧条目 |
| `simplify-asset-product-storage` | `assets` / Flat products have a checked versioned identity envelope | 新增 |
| `simplify-asset-product-storage` | `assets` / Checked product saves report the actual publication result | 新增 |
| `simplify-asset-product-storage` | `assets` / Loaded asset instances retain active data | 新增 |
| `simplify-asset-product-storage` | `assets` / Host World cooking consumes existing component metadata owners | 新增 |
| `simplify-asset-product-storage` | `physics` / Pinned and ABI-consistent Jolt dependency | 更新正文 |
| `simplify-asset-product-storage` | `physics` / SDK state and executable code outlive physics objects | 保留正文，仅补 2 场景 |
| `simplify-asset-product-storage` | `physics` / Authoring builders follow the existing developer build boundary | 新增 |
| `fix-ios-navigation-demo-linkage` | `physics` / iOS Jolt consumer RTTI parity | 新增 |
| `fix-ios-navigation-demo-linkage` | `platform` / iOS native framework linkage | 新增 |
| `fix-android-application-lifecycle` | `platform` / Standard Android Back routing | 新增 |
| `fix-android-application-lifecycle` | `platform` / Back preserves the system Activity policy | 新增 |
| `fix-android-application-lifecycle` | `platform` / Explicit exit drains lifecycle through destruction | 新增 |
| `fix-android-application-lifecycle` | `platform` / Polling distinguishes renderer drain and native lifecycle wait | 新增 |
| `fix-android-application-lifecycle` | `platform` / Same-process recreation preserves metadata and leases | 新增 |
| `fix-android-application-lifecycle` | `platform` / Overlapping Activities hand off engine session ownership | 新增 |
| `fix-android-application-lifecycle` | `platform` / Initialization failure retains late callback ownership | 新增 |
| `fix-android-application-lifecycle` | `platform` / Renderer terminal events remain observable during teardown | 新增 |
| `fix-android-application-lifecycle` | `modules` / Process-global metadata is registered in the process-once phase | 更新正文 |
| `fix-android-application-lifecycle` | `modules` / Tests have explicit metadata initialization owners | 更新正文 |
| `verify-ios-world-packaging` | `project-packaging` / World assets use checked binary products | 新增 |
| `verify-ios-world-packaging` | `project-packaging` / SPIR-V products omit optional source declarations | 新增 |
| `verify-ios-world-packaging` | `project-packaging` / World simulation is independent of presentation | 新增 |
| `verify-ios-world-packaging` | `project-packaging` / Output transforms and completed prepare ownership are single-use | 新增 |
| `verify-ios-world-packaging` | `project-packaging` / Launcher scene sessions have explicit ownership and events | 新增 |
| `verify-ios-world-packaging` | `project-packaging` / Native iOS launch and reproducible fixture | 新增 |
| `enforce-cmake-source-ownership` | `build` / Production translation units have one compile owner | 新增 |
| `enforce-cmake-source-ownership` | `build` / Exceptions preserve precise test contracts | 新增 |
| `enforce-cmake-source-ownership` | `build` / Ownership validation is automatic and regression tested | 新增 |
| `fix-sandbox-project-creation` | `editor-core` / Responsive project creation with progress | 新增 |
| `fix-sandbox-project-creation` | `editor-core` / Bounded project creation lifecycle | 新增 |
| `fix-sandbox-project-creation` | `editor-core` / Validated project creation result | 新增 |
| `refactor-vulkan-device-configuration` | `aurora-rhi` / Vulkan preferences are independent device configuration | 新增 |
| `refactor-vulkan-device-configuration` | `aurora-rhi` / Device capability resolution supports fallback | 新增 |
| `refactor-vulkan-device-configuration` | `aurora-rhi` / Disabled legacy dynamic APIs fail explicitly | 新增 |
| `refactor-renderer-runtime-flow` | `aurora-renderer` / Runtime preserves the ECS preparation boundary | 新增 |
| `refactor-renderer-runtime-flow` | `aurora-renderer` / Packet synchronization precedes ECS freezing | 新增 |
| `refactor-renderer-runtime-flow` | `aurora-renderer` / GPU orchestration preserves cleanup and completion order | 新增 |
| `refactor-aurora-runtime-service-lifecycle` | `aurora-renderer` / Renderer adapter is independent of Skeleton service publication | 新增 |
| `refactor-aurora-runtime-service-lifecycle` | `aurora-renderer` / Runtime assembly owns the Skeleton service session | 新增 |
| `refactor-aurora-runtime-service-lifecycle` | `aurora-renderer` / Service withdrawal and code lifetime are distinct | 新增 |
| `add-party-softbody-project` | `aurora-renderer` / Project scene input uses the launcher renderer owner | 新增 |
| `add-party-softbody-project` | `aurora-renderer` / Project arena is loaded through World | 新增 |
| `add-party-softbody-project` | `aurora-renderer` / Original character geometry is imported offline | 新增 |
| `add-party-softbody-project` | `aurora-renderer` / Collision response distinguishes impact and control state | 新增 |
| `add-party-softbody-project` | `aurora-renderer` / Close-up collision showcase | 新增 |
| `add-party-softbody-project` | `aurora-renderer` / Model-specific World2 spring demonstration | 新增 |

更新的 3 条正文分别是 Jolt 的平台 ABI/RTTI 契约，以及 Core metadata owner 与显式 context 初始化的两条契约。它们按当前代码核对；不重放旧的 DLL code lease。

Jolt lifetime 的两个补充场景为 `Independently linked native images use Jolt` 和 `Native and private implementation symbols could be coalesced`。活跃 `remove-module-code-leases` 的对应 delta 已同步补齐这两条，后续不会用较短旧块覆盖。

## 4 份草案移入 backlog（已确认并执行）

下面 4 个尚无 delta 的目录已整体移到 `openspec/backlog/`，全部 11 个原始文件与未完成状态保留，作为尚未展开的规划记录。它们不作为已交付 change 归档，也不为通过验证而补造契约。

| 草案 | 状态 | 条件 |
| --- | --- | --- |
| `animation-events` | planned | 事件/marker/state 设计与实现尚未展开 |
| `audio-world-integration` | planned | World 音频接入待设计 |
| `audio-animation-bridge` | blocked-on-prereq | 等待上述事件与 World 音频接入 |
| `aurora-cook-schema-layering` | debt-record | 分层方案尚未批准，未实施 |

上述两项已执行，活跃目录 **55 → 27**，backlog 保留 4 份草案；主 spec 仍为 32 份。其余冻结、债务和在做条目保留明确状态，不混为已完成。

## 执行记录与确认边界

1. 已重新检查 HEAD、当前正文与 delta 的 SHA-256，按依赖顺序应用上述选定操作；不复制旧版本整份 spec。
2. 已验证 1,390 条主需求、32 个领域及受保护正文；确认仅有清单内变动。
3. 已对 24 项使用 `openspec archive <change> --skip-specs --yes`，避免 CLI 再次同步把新契约覆盖为旧版；保留归档全部原始产物。
4. 已按批准整体搬移四个目录并保留内容；当前索引与引用随迁移更新。
5. 当前入口、状态表与验证记录同步更新；strict validation、路径与历史文件核验记录见 [verification.md](verification.md)。

仓库 [AGENTS.md](../../../AGENTS.md) 要求：**“openspec archive 前必须由用户确认：展示变更状态并等待明确同意，不得自动 archive。”** [change-hygiene skill](../../../.opencode/skills/openspec-change-hygiene/SKILL.md) 同样要求 **“Get explicit user confirmation before archiving”**。本轮已先展示清单并获得“确认，执行上述归档和草案整理”的明确答复，再按清单执行；commit 另需确认。

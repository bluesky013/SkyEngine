# 未展开草案

2026-11-01 经用户确认，将四份尚未形成 delta 的规划记录从 `changes/` 整体移入此处。11 个原始文件完整保留，未勾任务维持未完成；这些内容不是当前规范，也不表示已交付。当前接受的契约从 [领域索引](../README.md) 进入。

| 草案 | 状态 | 重新展开的条件 |
| --- | --- | --- |
| [animation-events](animation-events/proposal.md) | planned | 核对现有 Animation/GraphAssets；明确 marker、跨帧/循环事件及状态事件契约 |
| [audio-world-integration](audio-world-integration/proposal.md) | planned | 明确 World 创建/关闭归属、监听者与 source 更新，以及 runtime/editor 接线 |
| [audio-animation-bridge](audio-animation-bridge/proposal.md) | blocked-on-prereq | 等待上述事件分发和 World 音频接入；再确认骨骼 socket 查询边界 |
| [aurora-cook-schema-layering](aurora-cook-schema-layering/proposal.md) | debt-record | 先盘点真实符号依赖并批准 schema/target 归属，不默认采用旧草案方案 |

重新排期时将完整目录移回 `changes/`，先按最新代码重审 proposal 中的历史描述，补齐 design、canonical 领域 delta 和验收任务，再运行 strict validation。依赖这些草案的记录应链接此处，不创建同名的平行 change。

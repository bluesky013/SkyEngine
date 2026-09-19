# SkyEngine Agent Rules

## 确认规则

- **commit 前必须由用户确认**：给出拟提交内容（message + 文件清单）并等待明确同意，不得自动 commit。
- **openspec archive 前必须由用户确认**：展示变更状态并等待明确同意，不得自动 archive。

## 提交组成

- **同一 change 的 spec 与代码实现一起提交**：一次提交包含实现代码 + 该 change 的 openspec 产物（proposal/design/specs/tasks、archive 目录、`openspec/specs/` 主 spec 更新）。
- **不要拆出单独的 docs / archive 提交**（不再照搬历史的 `[feat]` + `[doc]: archive ...` 两段式）。
- 提交前缀按改动性质取一个（`feat` / `fix` / `refactor` / `build` 等），不要因含 spec 就改写成 `doc`。
- 仅纯规则/文档本身的修改（如本文件）可用 `doc`。

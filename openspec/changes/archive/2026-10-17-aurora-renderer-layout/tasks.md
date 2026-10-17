## 1. 结构纠正

- [x] 1.1 保存批前文件快照及路径映射，迁移 Renderer 接口/实现/测试到 Aurora core。
- [x] 1.2 合并实现 target，保留仅接口 target，收回内部 include 可见性。
- [x] 1.3 将 provider 注册接入现有 AuroraModule/Aurora.Render，迁移动态/静态模块测试。
- [x] 1.4 修正规则和相邻 change 的当前归属，固定 B10 连续收尾清单。

## 2. 验证与 review

- [x] 2.1 重新配置并构建 core/adaptor/runtime、Renderer 测试、SandboxModule/SandboxEditor。
- [x] 2.2 通过接口/CPU/资源/动态静态模块及原 core/adaptor 测试（13/13 组）。
- [x] 2.3 通过完整 GPU/RHI 回归并核对原生验证错误（GPU 592/592、RHI 133/133）。
- [x] 2.4 校验规范、文档与独立 diff，记录明确的已验证/未验证边界（见 review.md）。

用户于 2026-10-17 确认完整范围后，本 change 已与 renderer 实现一起归档、提交并 rebase；详见 [收口记录](../2026-10-17-aurora-renderer/closeout.md)。

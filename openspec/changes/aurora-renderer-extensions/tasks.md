> 2026-11-01 整理：**debt-record**。现行 owner：本 change 保留未排期扩展条件；基础平台验收归 `aurora-renderer-platform-parity`。
> 剩余：没有具体消费者与像素/生命周期验收前不激活；未来候选 delta 不得当作已实现契约同步。下方较早状态与验收记录保留为历史。

# 扩展债务

原阶段状态：debt-record（未排期）。接收目标设计，当前没有新增实现。

- [ ] 3.11 设计并接入可注册 CPU prepare feature、更多光类型/着色；保持 ECS AccessDesc、只读 View 与任务 drain，不能把 B13 GPU frame feature 当作已完成。
- [ ] 4.1 复审生产 FrameGraph feature 尚未覆盖的逐 View/HDR 扩展；仅为实际消费者补能力并验收真实像素与缓存保活，已删除的旧模板不再作为迁移目标。Global/Pass（View 可选）/Material/Batch 契约及 Pass 同源生成、PreparedRaster 固定 ABI 纠偏由 `fix-aurora-pass-resource-contract` 承接，本项不另定绑定模型。
- [ ] E1 在具体消费者确定后限定材质资产/HDR/tonemap 范围；不得宣称当前程序化 material 已覆盖完整资产链。
- [ ] E2 Compute/Custom 的声明访问与后端编码已由 `aurora-compute-custom-passes` 承接；本项只保留 storage/更多格式、跨队列和 GPU Scene 的候选评估，各自起独立实施范围。
- [ ] E3 对历史 Vulkan 初始化 0xC0000005 保留复现/dump 任务；已有重复通过不代表根因已解决。

2026-10-26 后续：用户确认无消费者即删除旧图。`remove-unused-legacy-rendergraph` 已移除旧执行器及其模板，4.1 不再承担这些已退役入口的兼容修复；其余候选扩展仍未因此完成。

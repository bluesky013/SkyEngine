# 扩展债务

Status: debt-record（未排期）。接收目标设计，当前没有新增实现。

- [ ] 3.11 设计并接入可注册 CPU prepare feature、更多光类型/着色；保持 ECS AccessDesc、只读 View 与任务 drain，不能把 B13 GPU frame feature 当作已完成。
- [ ] 4.1 复审并实现旧 pipeline 模板的显式逐 View 状态/HDR 迁移；同步本 change 的 aurora-pipeline delta，验收真实像素与缓存保活。
- [ ] E1 在具体消费者确定后限定材质资产/HDR/tonemap 范围；不得宣称当前程序化 material 已覆盖完整资产链。
- [ ] E2 在具体需求提出后分别评估 Compute/Custom、storage/更多格式、跨队列和 GPU Scene；各自起独立实施范围。
- [ ] E3 对历史 Vulkan 初始化 0xC0000005 保留复现/dump 任务；已有重复通过不代表根因已解决。

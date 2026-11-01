# 设计校验与 review 入口

日期：2026-11-01。状态：**Ready for user review；尚未实现生产代码。** 本报告只验证设计文档和一个有限预算模型，不替代 native IO、线程竞态、平台、性能或场景验收。

## 建议 review 顺序

1. [design.md](design.md)：架构、固定决策、导航 candidate 的明确成本与阶段边界。
2. [io-contract.md](io-contract.md)：公开操作、scope/interest/physical work 的区别、线性化与完整闭包预算。
3. [product-format.md](product-format.md)：v2 二进制布局、逻辑 BlobRef/物理 block、依赖 readiness、校验与 recook。
4. [industry-references.md](industry-references.md)：官方源码/文档支持什么、哪些约束是 SkyEngine 自己的设计选择。
5. [tasks.md](tasks.md) 与 `specs/`：实施/验收清单，全部实施任务仍未勾选。

## 本轮已执行

- `OPENSPEC_TELEMETRY=0 openspec validate add-common-async-io --strict`：通过。
- 检查本 change 的本地 Markdown 链接、尾随空白、文件结尾以及 capability 对应文件：通过。
- 用 Python `struct.calcsize` 核对 `<IIII16sQIIIIQ`、`<IHHQIIIIIIQ`、`<16sB3s`：prefix/block/dependency 分别 64/48/20 bytes。
- 最大 type name、dependency table、typed header 与 block table 同时取上限时，metadata 为 3,752,208 bytes，小于 4 MiB。
- 子任务分别复核行业主源、资产/渲染 owner 与对抗场景；已吸收 priority donation、stable current-load identity、共享 backing/owner ticket、control/handle 展开信用和跨 root 预算等待环修正。

没有修改生产源文件、构建脚本、当前产品或运行配置，没有 commit/archive。没有重新运行 C++、GPU 或真实 IO 测试。

## 有限预算状态模型

模型用两 root A/X、各一个必要 dependency B/Y，H=64 MiB。B/Y 各占 20 MiB，parent 额外需 30 MiB。每 root 的状态：

| 值 | 含义 | 实际 allocated |
| --- | --- | --- |
| 0 | metadata 完成，尚未开始 bulk | 0 |
| 1 | dependency 正在准备 | 20 |
| 2 | dependency ready，parent 未开始 | 20 |
| 3 | parent 正在准备 | 50 |
| 4 | root 完成，外部持结果 | 50 |
| 5 | 外部释放结果 | 0 |

旧策略仅在每一步分配时检查剩余空间。新策略在 0→1 前预留整个 closure 的 50 MiB，状态 1/2 为 A=20、R=30，状态 3/4 为 A=50、R=0。两种模型都允许 finite job 完成，状态 4 的外部结果可以释放。

用 breadth-first enumeration 检查所有可达状态：旧策略 24 个可达状态，发现 `(2,2)` 内部无法推进；新策略 20 个可达状态，没有这个模型内的内部等待死锁。旧反例路径为 `(0,0) → (1,0) → (2,0) → (2,1) → (2,2)`。

这个结果只支持“单 unit 预留不足，而 closure escrow 能消除此有限反例”。它不证明整个实现无死锁；没有模拟 native driver、线程内存序、required 图扩展、control/handle 数、取消竞态或永久不释放的外部缓存。对应真实/确定性测试仍见 tasks.md。

## review 时需重点权衡

- 公共 IoScheduler 与 storage backend 分离，首版选少量 blocking workers；native async 后端后续替换，公共协议先稳定。
- 完整 closure/control/handle 预留的第一版更保守，吞吐与利用率可能降低；优化不得恢复部分闭包互等。
- 产品 v2 需要 recook，第一版没有 v1 隐式 eager fallback；logical resource 可以跨 physical blocks。
- 导航完整 candidate 的单次成本随全部 resident state 增长，需能同时容纳 active/candidate/retired；它是正确性基线，不能被当作高效增量或无锁查询方案。
- 首批纹理/mesh 保持完整 CPU payload 与既有 uploader；GPU 部分 mip/LOD 驻留不在本轮交付范围。

实施前须完成用户 review。参数 profile 和平台 capability 仍需实测，本文没有把起始 worker/block 数值描述成最优值。

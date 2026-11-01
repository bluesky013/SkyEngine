# 寻路阶段性提交 review

日期：2026-11-01。状态：用户已明确授权 archive、commit、rebase 和 push；主 spec 已同步，change 已归档，归档产物与代码合入同一提交。

代码 review 时范围为 146 个路径；归档后最终精确范围为159个路径（M 68 / D 29 / A 62），包含9份主 spec 同步和本 change 的归档路径。精确路径与状态见 commit-files.txt，Git rename view 会合并部分移动条目。独立通用 IO 提案不纳入。

## 提交 message

```text
feat: complete navigation runtime and editor integration
```

精确文件清单见 [commit-files.txt](commit-files.txt)，实现和本 change 的 proposal/design/specs/tasks/verification 一起提交。新目录来自用户要求的核心/渲染、runtime/editor 分离以及实现归 plugin 的规则，不混入无关目录整理。

## 这次交付

| 范围 | 改动与原因 |
| --- | --- |
| engine/navigation | 保留领域接口、数据、factory dispatch 和纯空间辅助；消费者不依赖 Recast 实现 |
| plugins/recast | shared/runtime/backend/builder/render/editor 分 owner；离线/runtime 无 editor/render 反向依赖 |
| editor/render | 通用 active-world/overlay hooks，registry actions、延后构建、拥有数据的 Aurora debug bridge；未改 Aurora target/入口 |
| 基础设施与物理准备 | Task self-retain 收尾、loader-only drain、component readiness、collision mesh view；由真实导航 cook 缺陷驱动 |
| terrain bridge | 放在 provider plugin；Shutdown 在释放最后 world owner 前注销 provider |
| query | 每 call 独立 Detour context，多个共享读者可并行，mesh mutation 独占；异步 filter/终态/真实预算收尾 |
| streaming/codec | 内存 manifest 的所有 layer、坐标预算、fresh-first/backoff/清理、真实 pending；格式不变，分配前检查 |
| Framework / hosts | 默认内置 LZ4，宿主在加载模块前建立服务；按次 Acquire 保活 provider/code，删除 CompressionModule 装载与插件，保留静态实现唯一编译 owner |

## 并行 query 的结论

已经移除“共享一个 dtNavMeshQuery 导致所有 query 互斥”的限制。每次调用有独立 node pool/open list，共享只读 mesh 受 shared_mutex 保护；写操作仍独占。异步参数在提交时固定，取消后的 job 直到真正退出查询才归还额度，完成后的 Cancel 不会改写结果。

压缩消费链现在为 `Recast / AssetManager → Framework ICompressor → Framework 私有 LZ4`，无需注册/装载 CompressionModule。构建开关仍为 SKY_BUILD_COMPRESSION；关闭时未压缩产品正常使用，LZ4 请求显式失败。自定义测试/provider override 删除后恢复 builtin；获取的 shared handle 及可选代码租约活到操作结束。已有外部项目若配置旧模块项，应删除该项。

这版仍有读锁，writer 可能等待，shared_mutex 没有公平性承诺；每次 query 初始化 2,048-node context 有分配开销，目前没有性能基准。同步调用方须在调用期间保持输入/filter 不变，独占输出。更进一步的无 mesh mutex 查询仍需 immutable mesh generation、独立 context 和读者延迟回收，不在本次实现。

## 明确留待后续的内容

- 通用异步 IO、磁盘 range tile paging、产品 v2、闭包 credit 和 candidate/TryPublish 的设计保留在独立 `add-common-async-io` 提案，其整个目录不进入本次提交。
- 当前 Init 仍全量载入产品；Tick 同步安装/卸载 native tile。坐标预算不等于字节/毫秒预算，unload 尚无独立预算，focus 由显式调用者驱动。
- incremental ApplyBuild 失败可能保留已经应用的部分坐标，整批 native 替换没有事务回滚；失败会阻止发布成功资产。
- 不交付 crowd/动态障碍/off-mesh-link authoring、游戏控制器、需求驱动 GPU mip/LOD 驻留或独立 terrain 验收矩阵。
- `output/NavMeshValidation` 是本机验证项目，不包含在 commit 中；真实场景用例需要显式设置 fixture 环境变量。

## 验证与 review 重点

详细命令/证明范围见 [verification.md](verification.md)。最终所有已配置目标构建通过，14组回归通过；独立 LZ4 OFF 配置构建与接口/未压缩产品用例通过。干净场景重新烘焙得到11个产品；移出旧压缩 DLL 后，实际磁盘场景和3帧Metal编辑器 smoke 通过。第三方预检4项 metadata stale、输出齐全；没有伪称 fresh pinned build/Win32 验证。provider/code lease 已验证本地对象销毁顺序，未专门验证真实 DLL 卸载。

优先 review 领域/实现归属、跨模块 code lease 与 metadata owner、query 读写契约、取消/slot 生命周期、错误路径与现有格式兼容。文件多包含必要移动，Git 的 rename view 可帮助逐块查看实际修改。

用户已于 2026-11-01 明确授权本次提交及后续归档、rebase、push。OpenSpec archive 已同步20项新增、5项修改、4项删除的 requirement，归档目录为 `2026-11-01-aurora-navigation-integration`；不拆分单独的 docs/archive 提交。

已 rebase 到远端 `ab7ecc12`。唯一的 AssetTool 冲突同时保留上游 audio/terrain 开关判断与导航 builder/backend 依赖；全目标构建、19组回归、干净场景 cook/寻路和Metal编辑器 smoke 均通过，详见 verification.md。

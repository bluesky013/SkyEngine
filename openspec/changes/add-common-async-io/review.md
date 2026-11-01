# 当前资源读取 review 与外部参考

日期：2026-11-01。范围：当前工作树的 Core/Framework IO、导航 streaming/query、Aurora 生产场景纹理/材质/mesh/shader 链路。架构结论：**Risk**，缺少通用异步字节读取服务，已有 CPU 任务和 GPU 上传设施可以继续复用。本轮只审查并编写设计，没有修改生产代码或重新运行性能测试。以下行号对应审查时的工作树。

后续状态：同日 `aurora-navigation-integration` checkpoint 已修复导航 decoder 分配检查、异步 filter 快照/终态/实际存量、失败坐标退避和清理，并采用独立 Detour query contexts + 共享读锁。下列 findings 保留为设计触发证据；R1 的纹理部分、通用 IO/布局/转换问题仍待本提案实施。导航 writer 仍需独占保护，Tick 仍可能等待，不声称无锁或磁盘分页。

## 外部引擎参考

| 引擎 / 官方资料 | 已核实机制 | 对本引擎的设计启示 |
| --- | --- | --- |
| [Unreal FIoDispatcher](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Core/FIoDispatcher)、[FIoBatch](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Core/FIoBatch)、[FIoReadOptions](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Core/FIoReadOptions) | dispatcher 使用 chunk identity，backend 可挂载；batch 支持 priority/callback，read options 指定 offset/size | 把字节读取从资产类型中抽离；保留不同文件来源的实现入口。无需复制其容器格式。 |
| [O3DE IStreamer](https://www.docs.o3de.org/docs/api/frameworks/azcore/class_a_z_1_1_i_o_1_1_i_streamer) | range read 带 deadline/priority，支持请求批次与取消；取消完成不保证正在处理的目标已停止 | 背压、优先级和取消/实际完成应是公共请求契约；deadline 是调度目标，不应暗示硬实时保证。 |
| [Unity 6.0 texture/mesh loading](https://docs.unity3d.com/6000.0/Documentation/Manual/LoadingTextureandMeshData.html) | async 路径将 header 与大块二进制分开保存，后者经 ring buffer 跨帧上传；buffer 可能自动扩容 | 小元数据与大 payload 分离；读取预算和 GPU 上传预算分别管理；严格 hard cap 是 SkyEngine 自己的选择。 |
| [Godot background loading](https://docs.godotengine.org/en/stable/tutorials/io/background_loading.html) | request、status、get 分开，过早 get 仍会阻塞 | 非阻塞调用链必须贯穿到消费者，不能用后台请求后立即 wait 的路径证明 streaming。 |

这些是各引擎公开契约，最后一列是面向 SkyEngine 的设计推论，并非对其内部线程池实现的断言。更深入的源码核实与纠偏见 [industry-references.md](industry-references.md)：O3DE pending 无硬上限、Unity buffer 自动扩容，都不能当作本引擎硬背压的现成保证。

## 当前各资源实际怎么读

| 资源 | 生产链路 | 当前粒度与线程边界 |
| --- | --- | --- |
| 导航 | `NavigationSystem::Init` → `LoadAsset(ProductsOnly)` → `BlockUntilLoaded` → 保存全部 tile bytes → Tick 距离排序 → Add/RemoveTile | 初始化整包读取；Tick 同步解压/Detour 构建。现在分页的是 native tile 驻留，不是磁盘。 |
| 纹理 | packet producer → `SceneAssetSource` → `LoadAsset` → 轮询 loaded → `ConvertSceneImage` → CPU payload → `ResourceUploader` → GPU 完成 → Resident | 初次请求仍同步读产品；读取全部 mip 并组完整 CPU buffer；GPU 上传已有分块和预算。 |
| Mesh | `SceneAssetSource` → mesh product → 全部 vertex/index bulk → 一个 `RenderMeshLod` → CPU payload → 同一 uploader | 这条路径没有按需求 LOD 磁盘分页。 |
| 材质 | material/instance product → parent/texture 依赖 → schema/technique/variant → shader index/blob → material CPU payload | 元数据较小，但其依赖与 shader 文件读取可阻塞；材质不应该自建纹理 IO 池。 |
| Shader artifact | `ShaderResolver` → `ShaderCacheStore` → shader file system `ReadBin` | 绕过 AssetManager，同步读取 index/blob；开发模式还可直接执行编译。 |

可复核入口：

- `engine/aurora/adaptor/src/renderer/SceneAssetSource.cpp:565,958`：Load/default loader；`engine/aurora/adaptor/src/AuroraModule.cpp:190`：游戏 loader 虽使用 ProductsOnly，入口仍同步。
- `engine/aurora/adaptor/src/renderer/RenderSceneBridge.cpp:364`，`engine/sandbox/render/src/EditorRendererWindows.cpp:291`：读取请求来自场景 packet producer，不能假设已在 IO worker。
- `engine/aurora/adaptor/src/renderer/SceneImageConversion.cpp:24,37`：完整 mip 表、总 bytes 分配、逐 mip ReadBulk/复制；`SceneAssetSource.cpp:99,209`：完整 mesh bulk 和单 LOD 转换。
- `engine/aurora/cook/material/src/AuroraMaterialBuilder.cpp:50`、`AuroraMaterialInstanceBuilder.cpp:49`：纹理和 parent 依赖。
- `engine/aurora/core/src/renderer/RendererRuntime.cpp:141`、`resource/ResourceUploader.cpp:624`：上传预算和完成后发布。

旧 `engine/aurora/adaptor/src/ImageAsset.cpp:102,127` 的 `CreateTextureFromAsset` 确实等待 upload，但生产 SceneAssetSource 使用 CPU payload/uploader。不能据旧 helper 推断生产上传全部同步等 GPU。上述 Aurora 实现文件当前无本轮导航 diff，这些是既有链路限制。

## Findings

### R1 / P1：内部 count/size 在校验之前分配

- 边界：Framework decoder → navigation/image typed data。
- 证据：`plugins/recast/shared/src/NaviMeshAsset.cpp:42,102` 直接 resize；`engine/aurora/adaptor/include/aurora/adaptor/assets/ImageAsset.h:99` 直接用 sliceCount resize。
- 影响：小文件即使通过外层 checksum，也可声明巨额内部长度，在后续截断检查前尝试大分配。普通截断最终会被判 FAILED，问题是分配顺序和预算。
- 最小修正：按剩余字节、元素最小编码尺寸、领域最大数量/字节和总解压预算检查后分配。不要用 try/catch 或断言替代错误返回。

### R2 / P1：异步导航 filter 存在并发读写入口

- 边界：提交线程 → query worker。
- 证据：`plugins/recast/runtime/src/NavigationSystem.cpp:23` 保存可变 filter 指针；`plugins/recast/src/RecastQueryFilter.cpp:16` 的 setters 无同步，worker 才复制 flags/cost。
- 影响：提交后修改同一 filter 会改变排队请求，并可与 worker 读发生数据竞争。
- 最小修正：提交线程固定值快照，并明确导航服务容器和配置操作的 world-thread 契约。

### R3 / P2：异步入口不能消除持全局锁的同步整包 IO

- 边界：Core file → Framework asset orchestration → producer。
- 证据：`engine/framework/src/asset/AssetManager.cpp:379` 的 LoadAssetAsync 包装 LoadAsset；`:391` 的 loadMutex 覆盖文件打开、校验、typed/bulk 读取及依赖递归；`AssetProduct.cpp:75` 扫完整 body 校验后再读；`AssetManager.cpp:345` 预读全部 bulk。
- 影响：直接 LoadAsset 的调用线程会阻塞，多个 async 请求实际 IO 仍被全局锁串行。`AssetLoadRequest::resolved` 只代表获得 AssetPtr，不等于 CPU loaded 或 GPU resident。
- 最小修正：IO 在锁外调度；锁只保护 cache/in-flight 状态；先登记 in-flight 再发任务，防重复读取和 reload 迟到覆盖。

### R4 / P2：纹理/mesh 全量 CPU 拷贝，shader 另有同步路径

- 边界：Framework CPU asset → Aurora adaptor → shader store。
- 证据：`SceneImageConversion.cpp:37` 全量分配并逐 mip 临时复制；`SceneAssetSource.cpp:539` 自有缓存预算不含 AssetManager bulk。`:826` 同步准备 shader；`engine/aurora/core/src/aurora/shader/ShaderResolver.cpp:173,272` 读取 index，`:423` 开发编译；`ShaderCacheStore.cpp:220,343` 整文件 ReadBin。
- 影响：即便只换 AssetManager 的 loader，材质准备仍可能卡 producer；局部缓存限额无法约束所有 CPU bytes。
- 最小修正：shader 字节也走 Core 服务，index 按快照身份缓存；CPU 转换/编译独立调度；owned buffer/view 传递减少拷贝，GPU 上传保持原 owner。

### R5 / P2：坏 tile 重试饥饿与实际 in-flight 统计不一致

- 边界：导航需求调度 → 后台任务。
- 证据：`NavigationSystem.cpp:197,200,213` 最近失败坐标每帧优先耗预算，budget=1 时可永久挡住健康坐标；`:240` 取消后立即清 activeQueries，但 worker 仍运行；`runtime/include/navigation/NavigationSystem.h:42` pending 恒 0。
- 最小修正：显式失败与退避队列；实际任务终态前保留额度；分别报告 queued/in-flight/ready/resident/failed，失败重试不挤占所有新工作。

### R6 / P2：streaming Tick 与 query 仍会互相阻塞

- 边界：world Tick → Recast backend。
- 证据：`NavigationSystem.cpp:181,204` 同步装卸；`RecastNaviMesh.cpp:408,482,567` 共用 mutex；`:441` 锁内构建。取消只是标记，不是 reader drain。
- 影响：异步 query 仍可阻塞 Tick；只按坐标预算、卸载无预算，无法控制帧耗时。`SetStreamingFocus` 尚无生产 camera/agent 调用。
- 最小修正：先独立准备/发布阶段并补预算/宿主接入；无 mesh mutex 查询需要独立 query context、不可变 mesh 和延迟回收，另行评审。不能直接删锁。

### R7 / P2：已有 file/thread-pool API 不足以包装出可靠非阻塞服务

- 边界：Core。
- 证据：`engine/core/include/core/file/FileSystem.h:83` ReadData 返回 void；`engine/core/src/file/FileSystem.cpp:140` 每次重开路径且 offset 转 int。`engine/core/src/async/ThreadPool.cpp:188,213` 队列满时 yield 自旋；`engine/framework/src/asset/AssetExecutor.cpp:12` 的 loader/cook 各用 hardware_concurrency。
- 最小修正：新增 checked 64-bit ReadAt 与稳定 handle；公共队列先 admission，明确 BUSY；复用 `engine/core/include/core/async/Completion.h:34` 的取消、完成、指定 executor 和 drain 契约。

## 生命周期和已有正确边界

- 全量 OwnedBulkArchive 虽浪费内存，却保证旧实例不读到新产品；lazy IO 必须保留同样的稳定快照语义。`openspec/changes/archive/2026-11-01-simplify-asset-product-storage/design.md:29` 是当前契约，不能恢复旧 ProductGroup/持久 generation 目录。
- GPU fence、提交 pins、Resident 发布和资源退休继续归 Aurora；通用 IO 完成只能证明 CPU bytes 可读。
- 现有 Completion 已区分 cancelRequested 与终态，ContinuationExecutor 已有指定线程/容量/模块保活协议，公共 IO 应复用。

## 验证边界

本次为静态审查，没有给出新的实测延迟/吞吐结论。真实导航场景测试 `plugins/recast/test/RecastSceneTest.cpp:142` 关闭 streaming；真实后端分页测试只做单 Tick；runtime 假后端装卸不争用查询 mutex。因此此前通过的测试不能证明真实磁盘分页、并发装卸查询、永久错误公平性或帧时长上界。设计中的测试应使用可控阻塞/乱序读后端、真实 Recast 和现有 renderer 上传回执分别验收。

# 通用异步 IO 的行业参考与本引擎选择

核实日期：2026-11-01。本文只记录官方资料与源码，以及据此作出的 SkyEngine 设计选择。
O3DE 文档标为 26.05.0；源码链接指向 `development`，不是固定 commit，不能视为可重复构建快照。
Unity 使用 6000.0 文档；Unreal 使用当前公开 API 页面。网页可能更新，实现前应重新核对具体平台契约。
本文不构成吞吐、延迟或内存峰值的实测报告；实现状态以 [tasks.md](tasks.md) 为准。

## 来源确认：O3DE Streamer

### 调度线程与后端执行分离

`Scheduler::Start` 建立独立的 IO Scheduler 线程；主循环查询整个 stream stack 的 available slots，派发 prepared 请求、完成已结束请求，再推进执行。
它不是把所有文件操作直接扔进普通 CPU 池。[Scheduler 源码](https://raw.githubusercontent.com/o3de/o3de/development/Code/Framework/AzCore/AzCore/IO/Streamer/Scheduler.cpp)

跨平台 `StorageDrive` 是末端 fallback，公开声明 `s_maxRequests = 1`；scheduler 源码也说明默认 drive 并非完全异步。
Windows 专用节点另有 read slots、`ioChannelCount`、`overCommit` 和 `OVERLAPPED` 状态；这些证据不足以声称所有后端都使用 IOCP。
参见 [StorageDrive](https://www.docs.o3de.org/docs/api/frameworks/azcore/class_a_z_1_1_i_o_1_1_storage_drive)、[StorageDriveWin](https://www.docs.o3de.org/docs/api/frameworks/azcore/class_a_z_1_1_i_o_1_1_storage_drive_win)、[FileReadStatus](https://www.docs.o3de.org/docs/api/frameworks/azcore/struct_a_z_1_1_i_o_1_1_storage_drive_win_1_1_file_read_status)。

`ioChannelCount` 面向 IO 控制器的并发能力；少量 overCommit 可留待处理请求避免设备空闲，过大则减少重排机会；负值可避免占满设备。
这支持分别控制设备 active reads 与 scheduler lookahead，而非把 CPU 核数当作 IO 并发数。[Windows 节点参数](https://www.docs.o3de.org/docs/api/frameworks/azcore/class_a_z_1_1_i_o_1_1_storage_drive_win)

### deadline、priority 与局部性

`Thread_PrioritizeRequests` 区分控制命令顺序，并用 estimated completion 与 deadline 判断紧迫性；紧迫请求结合 priority/deadline，非紧迫请求考虑同文件和读取位置。
重排只影响尚未处理的请求。它是尽力调度，不能据此承诺硬实时 deadline。[Scheduler 源码](https://raw.githubusercontent.com/o3de/o3de/development/Code/Framework/AzCore/AzCore/IO/Streamer/Scheduler.cpp)、[IStreamer 契约](https://raw.githubusercontent.com/o3de/o3de/development/Code/Framework/AzCore/AzCore/IO/IStreamer.h)

### 解压阶段与内存生命周期

`FullFileDecompressor` 分别限制 `maxNumReads` 与 `maxNumJobs`；读取完成后保留压缩 buffer 等待解压，并使用专用解压 job manager，避免长解压占住普通任务系统。
官方明确：整文件压缩且没有 seek point 时，即使只请求一段也必须全读、全解；临时内存没有固定上限。
参见 [实现源码](https://github.com/o3de/o3de/blob/development/Code/Framework/AzCore/AzCore/IO/Streamer/FullFileDecompressor.cpp)、[FullFileDecompressor 限制](https://www.docs.o3de.org/docs/api/frameworks/azcore/class_a_z_1_1_i_o_1_1_full_file_decompressor)。

`RequestMemoryAllocator` 支持临近执行才分配，并提供 alignment、minimum/recommended size；`ClaimMemory` 显式移交结果所有权。
请求 completed 不代表 allocator 已不被内部使用，Lock/Unlock 对应其实际使用周期。[IStreamerTypes 源码](https://raw.githubusercontent.com/o3de/o3de/development/Code/Framework/AzCore/AzCore/IO/IStreamerTypes.h)

同一头文件明确：超过 active 建议数量的请求仍可 pending，总请求数没有硬限制；只建议音频/视频等洪泛型 consumer 自行控制数量。
因此 SkyEngine 的 Accepted/Busy、全局 byte cap、consumer 配额是本引擎新增约束，不能写成 O3DE 已保证。[Recommendations 契约](https://raw.githubusercontent.com/o3de/o3de/development/Code/Framework/AzCore/AzCore/IO/IStreamerTypes.h)

### 取消、通知与 drain

Cancel 命令完成不保证目标已停止；目标可能继续成功完成，调用方仍须处理目标自己的终态。完成 callback 在 TaskExecutor 线程运行，并非调用线程。
Scheduler 停止时继续处理 stack 至 idle；这不能替代宿主关闭外部请求入口并收齐通知的协议。
参见 [IStreamer 取消与回调契约](https://raw.githubusercontent.com/o3de/o3de/development/Code/Framework/AzCore/AzCore/IO/IStreamer.h)、[Scheduler drain](https://raw.githubusercontent.com/o3de/o3de/development/Code/Framework/AzCore/AzCore/IO/Streamer/Scheduler.cpp)。

## 来源确认：Unreal 的公开 API 边界

`FIoDispatcher` 按 chunk identity 读取，可挂载不同 backend；`FIoReadOptions` 表达 chunk 内 offset/size 和目标内存。
这支持把字节寻址从纹理、材质、导航类型中分离。[FIoDispatcher](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Core/FIoDispatcher)、[FIoReadOptions](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Core/FIoReadOptions)

`FIoBatch` 支持带 priority 的读取与完成 callback；`FIoRequest` 单独提供 status、result、cancel 和 priority 更新。
batch 调度不等于多个资产的原子发布；这些页面也未证明内部线程池结构、硬 byte cap 或使用 IOCP。
参见 [FIoBatch](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Core/FIoBatch)、[FIoRequest](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Core/FIoRequest)。

## 来源确认：Unity 的读取、上传与压缩布局

### 字节读取和取消

`AsyncReadManager.Read` 接受描述 offset、size、destination buffer 的 ReadCommand，返回 ReadHandle 查询状态；另有使用已打开 FileHandle 的入口。
这些是字节读取接口，不等于资源已经可绘制。[AsyncReadManager.Read](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/Unity.IO.LowLevel.Unsafe.AsyncReadManager.Read.html)

`ReadHandle.Cancel` 只取消仍在队列中的读取；已经开始的操作仍会完成。不能在 Cancel 返回时释放正在使用的目的内存。
这是 SkyEngine 取消与实际排空分离的直接参考。[ReadHandle.Cancel](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/Unity.IO.LowLevel.Unsafe.ReadHandle.Cancel.html)

### 小 header、大 payload 与上传缓冲

符合异步上传条件的纹理/mesh，其 header 写入 `.res`，大块二进制写入 `.resS`，后者通过 ring buffer 跨帧读取和上传。
该机制有资源资格与平台差异，不代表所有资源都自动走同一路径。[Texture and mesh loading](https://docs.unity3d.com/6000.0/Documentation/Manual/LoadingTextureandMeshData.html)

Unity 复用上传 ring buffer，并提供每帧 upload time slice；buffer 太小时会自动扩容以容纳当前最大资源，扩容本身可能带来耗时和额外内存。
因此不能把这个可配置 buffer 的初始值解释成不可突破的总内存上限。
参见 [上传配置](https://docs.unity.com/en-us/engine/6000.0/manual/analysis/graphics-performance-profiling/loading-texture-mesh-data-asynchronously/configure-asynchronous-upload-pipeline)、[asyncUploadBufferSize](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/QualitySettings-asyncUploadBufferSize.html)。

### 独立压缩的物理块

AssetBundle 的 LZMA 是整内容流压缩；LZ4 则独立压缩每个 128 KB 内容块，允许只读取和解压目标对象涉及的块。
这证明“有 range API”仍需匹配可独立解压的物理布局；不意味着导航 tile 或单个 mip 必须恰好等于一个物理块。
参见 [AssetBundle compression formats](https://docs.unity3d.com/6000.0/Documentation/Manual/AssetBundles-Cache.html)。

## 来源确认：Windows 原生异步读取的最低契约

`ReadFile` 返回 FALSE 且错误为 `ERROR_IO_PENDING` 表示异步操作尚未完成，并非读取失败；也可能直接同步完成。
目的 buffer 和 OVERLAPPED 在实际完成前不能释放、修改或复用；过多 outstanding IO 还可能耗尽内存或锁页额度。
即使 handle 使用 OVERLAPPED，系统调用也可能阻塞，因此不能把原生 read/open 直接放到要求无磁盘等待的 producer。
以上均来自 [Microsoft ReadFile 文档](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-readfile)。

`CancelIoEx` 只发出取消请求，不等待操作结束；目标最终可能正常完成、以 ERROR_OPERATION_ABORTED 取消，或以其他错误失败。
取消 API 返回后仍须等待目标完成，才能回收 buffer/OVERLAPPED 和实际 active 额度。[Microsoft CancelIoEx 文档](https://learn.microsoft.com/en-us/windows/win32/api/ioapiset/nf-ioapiset-cancelioex)

## 本引擎选择：采纳机制与保留边界

下表是 SkyEngine 的设计决定，不是对上述引擎内部实现的附加断言；完整契约见 [design.md](design.md)。

| 参考机制 | 采纳 | 不采纳 / SkyEngine 契约 |
| --- | --- | --- |
| O3DE scheduler/backend 分离 | 单一调度 owner、按 backend 控制 active IO | 不为 feature 各建池；blocking worker 数与未来设备 queue depth 分开 |
| deadline/priority/locality | 控制通道保留、soft deadline、公平与 aging | 不承诺硬 deadline；初版不复制未经测量的预测模型 |
| O3DE 延迟分配与所有权交接 | 执行前分配、owned buffer/view、实际结束后回收 | 全链路 byte lease、consumer 配额与峰值 reservation 由本引擎额外保证 |
| O3DE 分离 read/decode 槽位 | 读取和 CPU 准备分别限流 | 不复制整文件无界解压或每 feature 解压线程池；避免跨阶段 hold-and-wait |
| Unreal chunk/range/backend | Framework 将领域键映射为可寻址块，Core 只处理字节 | 不引入 Unreal 容器格式，不把 batch 当多资产事务 |
| Unity 独立物理压缩块 | 大逻辑资源可拆成多个有上限、独立校验/解压的块 | 不照搬固定 128 KB；块大小由放大率、设备与解码峰值实测确定 |
| Unity 跨帧上传 | 保留 Aurora ResourceUploader、fence 和退休 owner | IO 完成不代表 GPU Resident；不把自动扩容称作硬内存预算 |
| O3DE/Unity/Windows 取消语义 | cancel requested、目标 drained、通知 drained 分开 | 保留稳定 handle、buffer、预算及 backend 实例到真实生命周期结束；SkyEngine 的 DLL 代码由 loader 保持进程常驻 |

稳定产品 snapshot、flat UUID 原子替换、旧实例继续读取旧数据，属于 SkyEngine 已有契约；以上资料不替代该约束。
本提案不声称任何参考引擎已同时保证无阻塞 admission、全局硬 byte cap、无锁查询与全平台原生异步 IO。

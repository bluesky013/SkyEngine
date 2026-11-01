# Flat product v2 读取布局

状态：2026-11-01 设计契约，尚未实现。继续使用 `<uuid-prefix>/<uuid>.bin`；UUID 是产品定位，instance/revision 是进程内实例身份。这里的 block 是独立压缩物理单元，BlobRef 是 typed header 中的逻辑资源，不引入另一套容器、ProductGroup 或持久 generation 目录。

## 1. 物理块与逻辑资源

```text
64-byte prefix | bounded metadata | alignment | block 1 | ... | block N
                     |
                     +-- typed header: mip / LOD / tile-layer -> BlobRef
                                                               |
                                                               +-- ordered BlockSlice[]
```

一个 mip/LOD/tile-layer 可以跨多个 block；多个小逻辑资源也可引用同一 block 的不同 decoded ranges。定位和解压放大以物理 block 为单位度量。第一版只合并相同 block 的读取/解压，不做跨文件压缩或任意物理邻接合并。

## 2. 固定 prefix：64 bytes

所有整数 little-endian，按字段编码，禁止用本机 struct 布局直接落盘。

| Offset | 类型 | 字段 / 规则 |
| --- | --- | --- |
| 0 | u32 | magic，沿用 `0x44525053` |
| 4 | u32 | formatVersion = 2 |
| 8 | u32 | prefixBytes = 64 |
| 12 | u32 | flags = 0；未知位拒绝 |
| 16 | byte[16] | product UUID，必须匹配请求身份 |
| 32 | u64 | fileBytes，必须等于同一 read handle 的文件长度 |
| 40 | u32 | metadataBytes，不含 prefix |
| 44 | u32 | blockCount |
| 48 | u32 | metadataCrc32c |
| 52 | u32 | prefixCrc32c：计算时本字段置零 |
| 56 | u64 | reserved = 0 |

CRC32C 复用 `engine/core/include/core/crypto/crc32/Crc32.h`，只提供损坏检测，不提供认证或抗恶意篡改保证。prefix CRC 覆盖全部 64 bytes，包括 metadata CRC；metadata CRC 覆盖 metadata 原始字节，包括依赖、typed header 和 block table。

读取顺序固定：64-byte prefix → 校验固定字段和 CRC → 检查 limits/长度算术 → 取得 metadata 控制预算 → 读取并校验 metadata → 解析。prefix 声明值不能直接驱动无限分配。

## 3. Metadata 编码顺序

metadata 不压缩，使控制路径的解码峰值可直接界定。

1. `u32 typeNameBytes`，随后为不含 NUL 的 UTF-8 type name。
2. `u32 assetSchemaVersion`：由 owning handler 检查，和产品布局版本分开。
3. `u32 dependencyCount`，随后 `DependencyEntry[dependencyCount]`。
4. `u32 typedHeaderBytes`，随后不透明 typed header bytes。
5. `BlockEntry[blockCount]`，数量来自 prefix。

metadata 必须精确消费，尾随未解释字节视为 InvalidData。解析 table 前以 checked multiplication 验证其编码长度可容纳；typed header 可以借用 immutable metadata backing，不强制复制。

### DependencyEntry：20 bytes

| Offset | 类型 | 字段 |
| --- | --- | --- |
| 0 | byte[16] | dependency UUID |
| 16 | u8 | readiness kind：0 = CpuRequired，1 = MetadataRequired，2 = Reference |
| 17 | byte[3] | reserved = 0 |

- CpuRequired：当前产品的 CpuReady 等待目标 CpuReady。材质 parent 等属于这一类。
- MetadataRequired：当前产品的 CpuReady 等待目标 MetadataReady；不会触发目标 bulk。需要 typed descriptor 时应使用 CpuRequired，而不是假定 generic metadata 已是 typed 对象。
- Reference：runtime 按需解析，不阻塞当前 CpuReady；packaging 仍需遍历引用闭包。适合大世界分区页与尚不需要的资源引用。

同 UUID 不重复编码不同 kind，producer 合并为所需最强 kind。零 UUID、非法 kind、重复项拒绝。CpuRequired 自引用拒绝；MetadataRequired 自引用可被 writer 规范化省略；Reference 不参与 CPU 等待环检测。MetadataReady 在该产品自己的 prefix/metadata 校验完后发布，不等待这些依赖。

### BlockEntry：48 bytes

| Offset | 类型 | 字段 |
| --- | --- | --- |
| 0 | u32 | blockId，连续为 1..blockCount |
| 4 | u16 | codec：0 = None，1 = LZ4 block |
| 6 | u16 | flags = 0 |
| 8 | u64 | fileOffset，产品文件内绝对 offset |
| 16 | u32 | storedBytes |
| 20 | u32 | decodedBytes |
| 24 | u32 | storedCrc32c |
| 28 | u32 | decodedCrc32c |
| 32 | u32 | alignment = 16，第一版固定 |
| 36 | u32 | reserved = 0 |
| 40 | u64 | reserved = 0 |

block 按 id/物理 offset 同序排列。第一个 offset 为 `alignUp(64 + metadataBytes, 16)`；后续为 `alignUp(previousOffset + previousStoredBytes, 16)`。最终 fileBytes 等于最后一个 block 的结束位置；零 block 产品的 fileBytes 为 `64 + metadataBytes`。padding 由 writer 填零；runtime 无须为校验 padding 扫描未请求的区域，deep audit 会检查。

storedBytes 和 decodedBytes 必须非零。None 要求二者相等，两个 CRC 也相等。LZ4 使用独立 block，不依赖前块 dictionary；writer 仅在压缩后更小时选 LZ4，否则使用 None。解压必须得到精确 decodedBytes。未知 codec 返回 Unsupported，校验或长度不符返回 InvalidData，不能成功返回部分内容。

### BlobRef / BlockSlice

通用建议编码为 `u64 logicalBytes + u32 sliceCount + BlockSlice[sliceCount]`；每个 slice 为 `u32 blockId + u32 decodedOffset + u32 byteCount`。切片顺序就是逻辑字节顺序。

解析必须确认：blockId 存在；byteCount > 0；decodedOffset/byteCount 不越 block 边界；所有 byteCount 的 checked sum 等于 logicalBytes；空逻辑资源只允许零 slices。typed schema 可以增加领域信息，但必须复用等价边界检查。BlobRef 的 logicalBytes 不自动申请连续大 buffer：streaming consumer 可以逐窗口消费；当前 Aurora 完整 payload 路径则必须先取得完整 gather 预算。

## 4. 首版 reader 限额与 writer 策略

这些是初始协议支持范围，超限返回明确错误并由 producer 拆分产品/逻辑准备单元，运行时不自动扩容。

| 项目 | 首版上限 / 策略 |
| --- | --- |
| type name | 256 bytes |
| metadata | 4 MiB |
| typed header | 512 KiB |
| dependencies | 4,096 |
| blocks | 65,536，仍必须满足 metadata 总上限 |
| decoded block | 1 MiB |
| stored block | <= decodedBytes <= 1 MiB |
| writer 目标 decoded block | 256 KiB 起始策略，最终由读放大/解压/设备测试调整 |
| 总文件 / BlobRef offset、size | u64；还须满足 backend 实际平台可表示范围 |

大导航世界按区域分成多个 UUID 产品，小 root/index 使用 Reference 边按需打开区域；不能通过取消 metadata 上限支持无限 manifest。纹理 mip、mesh LOD 不等同于单物理 block，产品分块也不自动提供 GPU 部分驻留。

以上 256 KiB 是 SkyEngine 起始策略，不是照抄 Unity 的 128 KB，也不是测出的最优值。block 大小和服务底层 transfer quantum 独立：一次 block 请求可拆成多个 native read segment。

## 5. 两级校验与快照

- runtime MetadataReady：已验证 prefix、metadata 与所有 block ranges，不读取无关 payload。不能据此报告全文件内容已检查。
- 请求 block：同一 snapshot 上读取 stored bytes → 校验 stored CRC → 按预算解压 → 验证 decoded 长度/CRC → 发布 immutable backing。
- deep audit：验证全部 blocks、padding、typed schema 与引用闭包；用于 cook readback、打包和显式完整检查。
- cached block key 包含 snapshot identity、blockId、codec/representation version；不得仅用 UUID 或 CRC 作为唯一身份。

引擎 writer 只发布已关闭、校验完成的临时文件，并原子替换路径。旧只读 handle 保留旧对象。原地修改/截断不在 ImmutableVersion 来源契约内；对 mutable shader/source，准备过程发现版本/内容不符时整体 SourceChanged/InvalidData，不能把新内容作为旧 snapshot 发布。文件大小、mtime、fileId 只用于诊断/变更提示，不是内容不可变证明。

## 6. 迁移与错误边界

v1 和旧 typed schemas 显式 Unsupported，清理受影响产品并 recook；同步工具与异步 runtime 使用相同 v2 reader/checks。writer/reader 成套切换，失败发布不替换旧产品。固定 prefix/schema/codec 不匹配不触发 runtime cook；ProductsOnly 始终禁止 cook。

跨 UUID 的 per-file 替换仍不构成多产品事务。Mesh/Skin/Skeleton、材质 parent/override、nav agent/build 参数的兼容检查继续由领域验证。校验失败不得覆盖当前 active CPU/GPU/native 资源，也不允许无限每帧重试。

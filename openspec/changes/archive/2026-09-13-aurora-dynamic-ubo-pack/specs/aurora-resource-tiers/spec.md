# aurora-resource-tiers Specification Delta

## MODIFIED Requirements

### Requirement: Batch tier dynamic UBO

`BatchAllocator` SHALL 管理 batch tier（set 2）的 dynamic UBO：纯线性分配器，`Allocate(size) -> offset` 按 `DeviceCapability::minUniformBufferOffsetAlignment` 对齐分配，`Write(offset, data, size)` 写入，`Reset()` 清 cursor；不感知帧，in-flight 安全由 `DeviceFrameContext` 维护 per-inflight-frame 的 pack buffer。

batch RG 的 dynamic UBO descriptor SHALL 以 `offset=0, range=blockSize` 绑定（每帧一次，帧内不变）。`DrawItem` SHALL 含 `batchDynamicOffset`（其值为 allocator 产出的 pack offset）；executor 绑 set 2 SHALL 传 dynamicOffsets。

#### Scenario: 两个 draw 共用 RG 不同 offset

- **WHEN** 两个 DrawItem 同 batchRG、不同 `batchDynamicOffset`
- **THEN** executor 两次 `BindResourceGroup(2, rg, 1, &offset)` 分别使用各自 offset

#### Scenario: per-inflight-frame 不覆盖

- **WHEN** FrameContext 维护多个 pack buffer，连续帧各自分配写入并提交
- **THEN** 当前帧写入的 buffer 与仍在 GPU 读取的帧 buffer 不同，不覆盖

## Why

Status: active. 用户明确拒绝 Renderer 再建立 core/adaptor/runtime 子模块体系，并确认回归 Aurora 现有职责归属。此前 CPU/GPU 依赖隔离被扩成重复模块，Renderer adaptor 与已有 Framework adaptor 含义不同，另一个 runtime 入口也增加了生命周期分歧。

## What Changes

- Renderer 接口、实现和测试迁入现有 Aurora core；公开 `<aurora/renderer/...>` 路径不变。
- 删除 Aurora.Renderer.Static/Adaptor 实现 target 和独立 Aurora.Renderer 模块，保留仅接口 target。
- Framework 注册并入 AuroraModule，沿用 Aurora.Render 动态/静态入口；拒绝重复 owner，卸载撤销自身服务。
- 实现头改为 private include；CPU prepare 仍不调用 GPU/Framework。
- 修正规则和 aurora-renderer/aurora-scene-bridge 的当前归属描述。

## Capabilities

### Modified Capabilities

- `aurora-adaptor`: Renderer foundation 实现与 Framework 注册的既有模块归属。

## Impact

只纠正已确认的结构；不迁移 RHI 后端，不改变 B10 GPU 功能，不实现生产 Renderer。功能协议仍由 aurora-renderer 拥有；World 提取由 aurora-scene-bridge 拥有。移动清单、批前快照与独立 diff 保留供 review；commit/archive 仍需用户确认。

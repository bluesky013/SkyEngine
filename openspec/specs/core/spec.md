# core Specification

## Purpose

Define core data structures, entity storage, graph operations and frame-local allocation contracts.

## Requirements

### Requirement: EntityId

`EntityId` SHALL 为 32 bit：`24 bit index | 8 bit generation`。generation 在 entity 销毁后自增；持旧 generation 的引用访问 SHALL 失败。generation 耗尽后对应 index SHALL 退休，不得回绕；索引空间耗尽时 CreateEntity SHALL 返回 INVALID_ENTITY。最高 index SHALL 保留，以避免生成无效 sentinel。

#### Scenario: generation 防悬垂
- **WHEN** entity 销毁后其 index 被新 entity 复用
- **THEN** 旧 EntityId 的 Get 返回 nullptr、Contains 返回 false

#### Scenario: generation 耗尽
- **WHEN** 某 index 的最大 generation 实体被销毁
- **THEN** 后续创建使用其它 index，所有旧 generation 均不恢复存活

### Requirement: SparseSet 容器

`SparseSet<T>` SHALL 提供 sparse 索引表与对齐的 dense entity/data 数组，以及 Add、Remove、Get、Contains 和连续迭代。T SHALL 可移动构造及赋值，组件操作 SHALL 不主动抛异常。Add 成功 SHALL 返回组件指针；对同一 EntityId 重复 Add SHALL 更新其唯一 dense 项；非法 sentinel 或同 index 的其它 generation 仍占用时 SHALL 返回 nullptr 且不修改现存组件。swap-remove SHALL 在组件移动后更新实体索引映射；标准库分配异常当前不处理。

#### Scenario: dense 连续迭代
- **WHEN** 多次 Add/Remove 后遍历 dense data 数组
- **THEN** 数组无空洞，顺序扫描不遇指针跳转

#### Scenario: swap-remove 保持映射正确
- **WHEN** 删除中间元素
- **THEN** 末尾元素换入被删槽位，其 sparse 项同步更新，所有存活 entity Get 结果正确

#### Scenario: 重复添加和 generation 别名
- **WHEN** 同一实体被重复添加，随后有人以另一 generation 添加相同 index
- **THEN** 前者更新唯一组件，后者被拒绝且不破坏当前映射

### Requirement: EntityRegistry

`EntityRegistry` SHALL 提供 CreateEntity、DestroyEntity、按组件类型懒建 SparseSet 池、Add、Get 和 Remove。DestroyEntity SHALL 从所有池删除该实体，删除后才回收其 slot；重复销毁旧 ID SHALL 无副作用。存活状态 SHALL 独立于 generation，尚未重新 Create 的 free slot 不得被猜测的下一 generation 激活。Add 成功 SHALL 返回组件指针，到死亡或过期实体 SHALL 返回 nullptr 且不懒建池；Get/Remove 不得隐式创建不存在的池。

组件池索引 SHALL 使用 TypeId 的显式 SKY_TYPE_TAG constexpr FNV-1a 哈希，不得使用函数内 static、全局计数器或签名 fallback；未注册类型 SHALL 编译失败。

#### Scenario: entity 生命周期
- **WHEN** CreateEntity → Add 组件 → DestroyEntity
- **THEN** 所有组件池对应项被移除，已有 View 不再遍历该实体；未耗尽的 index 后续可用新 generation 复用

#### Scenario: 迟到更新和 free slot
- **WHEN** 旧 generation 的更新到达，或调用方在 Create 前猜测下一 generation
- **THEN** Add 被拒绝、Get 返回 nullptr，当前替代实体与 free list 保持有效

#### Scenario: 跨平台/编译器/模块类型 id 一致
- **WHEN** 同一类型在不同模块或 toolchain 调用 TypeId
- **THEN** 返回值由显式 tag 决定且保持一致

#### Scenario: id 与调用顺序无关
- **WHEN** 两个模块以不同顺序首次查询 A/B 的 TypeId
- **THEN** 同类型 id 在两模块保持相同

#### Scenario: 未注册类型编译失败
- **WHEN** 对未打 SKY_TYPE_TAG 的类型调用 TypeId 或 Pool
- **THEN** static_assert 在编译期拒绝

#### Scenario: 哈希碰撞检测
- **WHEN** debug 构建下池查找命中相同 hash、不同 tag
- **THEN** 断言失败

### Requirement: 通用组件与资源访问描述

Core ECS SHALL 提供与渲染无关的访问描述数据，支持 Read/Write、组件和具名 CPU 资源；Write SHALL 包含读改写语义。访问键 SHALL 包含 domain、带 generation 的 owner 实例与类型/资源身份。组件类型 SHALL 复用显式 SKY_TYPE_TAG / TypeId，并保留 tag 碰撞检查；不同 owner 的同类型组件不得误判冲突。间接访问的共享存储 SHALL 使用共同的资源 key 声明，帧序号不得将同一可变存储伪装成不同资源。

#### Scenario: 不同实例与同实例
- **WHEN** 两个 system 分别写 registry A/B 的同一组件类型，而另两个 system 读同一 registry 的该组件
- **THEN** A/B 的写访问不冲突；同一实例的 Read/Read 不冲突；同一实例的 Read/Write 或 Write/Write 冲突

#### Scenario: 组件引用共享 CPU 资源
- **WHEN** 不同组件中保存的句柄指向同一份可变 CPU 资源
- **THEN** systems 显式声明共同资源 key，依赖推导按该资源处理冲突

### Requirement: 确定性访问依赖编译

Core SHALL 提供从 system 描述到 DependencyPlan 的纯编译函数，不依赖 Renderer、RHI 或线程池。描述 SHALL 包含稳定 system identity/order、phase、访问集合与可选 Before/After。编译 SHALL 先验证显式顺序并作稳定拓扑排序，再按此逻辑顺序为重叠 key 推导 RAW/WAR/WAW：reader 等待 lastWriter，writer 等待 lastWriter 与之前所有未被后续 writer 覆盖的 readers。Read/Read SHALL 不建立数据依赖。同 system 的重复访问 SHALL 合并为最强模式，不产生自依赖。环、未知依赖、无效身份或逆 phase 约束 SHALL 在执行前拒绝并给出诊断。

#### Scenario: 多个 readers 后写入
- **WHEN** 逻辑顺序为 W0、R1、R2、W3，四者访问同一资源
- **THEN** R1/R2 都等待 W0 且可并行，W3 等待两者及必要的先前 writer 依赖

#### Scenario: 声明本帧 producer
- **WHEN** 一个 reader 必须消费某 writer 的本帧结果
- **THEN** 调用方通过后续 phase 或显式 After 表达该顺序；编译器不凭 Read/Write 猜测并反转业务顺序

#### Scenario: 顺序稳定与错误诊断
- **WHEN** 相同 system 描述以不同注册插入顺序交给编译器，或其中存在显式环
- **THEN** 有效描述生成相同计划；环被拒绝并指出 system 与边来源，而非提交永远无法完成的任务

### Requirement: 结构冻结后的受限访问

ECS SHALL 提供预绑定的只读与受限可写视图，供调度器在结构冻结区间使用。Read SHALL 返回 const 数据，Write SHALL 只允许更新已存在且被授权的组件元素。进入冻结前 SHALL 串行完成池创建、组件预分配与 query 绑定。冻结期间 SHALL 不允许 Add/Remove/Create/Destroy、池懒创建或导致地址/索引改变的容器操作。受限视图 SHALL 显式拒绝未声明的访问并使活跃任务失败；普通裸 registry API SHALL 不被宣称自动线程安全。

#### Scenario: 未创建组件池的 query
- **WHEN** prepare 的声明包含尚未存在的组件池
- **THEN** coordinator 在冻结前完成必要初始化或绑定为空视图，worker 不调用 Pool 创建它

#### Scenario: 只读任务越权
- **WHEN** 仅声明 Read 的任务通过受限视图请求修改或未声明的资源
- **THEN** const 类型约束或运行时访问校验拒绝该访问，返回无效视图或 false，并使活跃任务失败

### Requirement: 保守冲突粒度与可分块写入

不同 systems 之间 SHALL 首先按 owner 内完整组件池或资源判断冲突；dirty/tag/filter 不得被直接当作互斥证明。只有被声明为 element-wise 且持有唯一写范围的 kernel SHALL 使用可写分块视图；chunk SHALL 不越界写其它实体。整个 system 的依赖完成 SHALL 覆盖其所有 chunks 与输出归并。

#### Scenario: 不同过滤条件写同一组件
- **WHEN** 两个 systems 用不同 tag 查询并写同一 registry 的 Bounds
- **THEN** 保守建立写冲突依赖，不假定两个查询集合互斥

#### Scenario: 分块完成屏障
- **WHEN** 同一 element-wise system 的一个 chunk 完成而其它 chunks 仍运行
- **THEN** 下游对该组件的访问尚未被释放；所有 chunks 和必要归并完成后才可运行

### Requirement: ECS View 多池交集迭代

`EntityRegistry` SHALL 提供 `View<Ts...>()`，返回多组件池交集视图。迭代 SHALL 以最小组件池为主驱动（dense 连续扫描），其余池以 `Contains` 检查交集，命中回调 `fn(EntityId, Ts&...)`（各组件引用）。

#### Scenario: 两池交集
- **WHEN** entity e1 有 A+B，e2 只有 A，`View<A,B>().ForEach(fn)`
- **THEN** fn 只对 e1 触发，参数为 e1 的 A、B 引用

#### Scenario: 最小池驱动
- **WHEN** A 池 1000 项、B 池 3 项，`View<A,B>()`
- **THEN** 迭代扫描 B 池 dense 数组（3 次 Contains 检查），不扫 A 池

#### Scenario: 单组件 View
- **WHEN** `View<A>()`
- **THEN** 等价于直接扫 A 池 dense 数组

### Requirement: Core provides a self-contained graph utility

`engine/core` SHALL provide a header-only directed-graph utility with vertex creation, edge creation, and
topological sort, without depending on Boost.

#### Scenario: Topological order puts dependencies first

- **WHEN** edges are added as `from -> to` meaning `from` depends on `to`
- **THEN** the topological order SHALL list `to` before `from` for every edge

#### Scenario: Duplicate edges

- **WHEN** the same edge is added more than once
- **THEN** it SHALL be treated as a single edge and SHALL NOT affect the order

#### Scenario: Empty and single-vertex graphs

- **WHEN** the graph has no edges
- **THEN** the topological order SHALL contain every vertex exactly once

### Requirement: ModuleManager orders modules with the Core graph

`ModuleManager` SHALL use the Core graph utility instead of Boost, preserving load and unload ordering.

#### Scenario: Load order

- **WHEN** modules are registered with dependencies
- **THEN** modules SHALL be loaded with dependencies before dependents

#### Scenario: Unload order

- **WHEN** modules are unloaded
- **THEN** dependents SHALL be shut down before their dependencies (reverse of the load order)

#### Scenario: No Boost in ModuleManager

- **WHEN** the framework module is built
- **THEN** `ModuleManager` SHALL NOT include or link Boost

### Requirement: FrameAllocator 提供帧作用域 bump 分配

`FrameAllocator` SHALL 提供非虚 bump 分配接口，基于 `LinearStorage` 块链管理。分配操作 O(1)，无每对象元数据。

- `Allocate(size, alignment) -> void*` — 从当前 block bump 分配；当前 block 不足时自动扩展新 block
- `AllocateArray<T>(n) -> T*` — 分配 `n * sizeof(T)` 对齐到 `alignof(T)`
- `Construct<T>(args...) -> T*` — bump 分配后 placement-new 构造
- `Arena() -> TransientAllocator&` — 返回内部 `TransientAllocator` 视图，供 `TransientStdAllocator<T>` / `MakeTransientVector` 等使用
- `GetMark() / Rewind(Mark)` — 检查点/回退（见 Requirement: 检查点与回退）
- `Reset()` — 帧末整体回收所有 block

`FrameAllocator` 为单线程使用。统计接口见 Requirement: 内存统计。

#### Scenario: bump 分配连续地址
- **WHEN** 依次 `Allocate(16)` 和 `Allocate(32)`
- **THEN** 两次返回地址在同一 block 内连续（或跨 block 扩展），无 free list 查找

#### Scenario: Construct 构造对象
- **WHEN** `Construct<std::pair<int, float>>(42, 3.14f)`
- **THEN** 返回构造完成的对象指针，对象位于 bump arena 内

### Requirement: 检查点与回退（Mark/Rewind）

`FrameAllocator` SHALL 支持 `GetMark()` 记录当前分配位置，`Rewind(Mark)` 回退到该位置。回退后 mark 之后分配的所有内存被逻辑回收，后续分配可复用空间。

- `Mark` 记录当前 block 索引 + block 内 offset
- `Rewind` 丢弃 mark 之后的 block，回退 mark 所在 block 的 offset
- `Rewind` 后 `mCurrentBytes` 相应回退

#### Scenario: setup 图编译后回收
- **WHEN** `Mark m = alloc.GetMark()`；建图/编译分配大量内存；`alloc.Rewind(m)`
- **THEN** mark 之后分配的空间被回收，后续 compiled graph 分配复用该空间

#### Scenario: 跨 block 回退
- **WHEN** 分配跨越多个 block 后 `Rewind` 到较早 mark
- **THEN** mark 之后的 block 被丢弃，mark 所在 block 的 offset 回退，无内存泄漏

### Requirement: 标准容器适配

`FrameAllocator` SHALL 通过 `Arena()` 提供 `TransientAllocator&`，配合 `TransientStdAllocator<T>` 使标准容器（vector/list/map/string）从 bump arena 分配。

- `TransientVector<T>` 别名 SHALL 存在
- `MakeTransientVector<T>(alloc)` 工厂 SHALL 存在

#### Scenario: TransientVector 从 arena 分配
- **WHEN** `auto vec = MakeTransientVector<int>(alloc); vec.emplace_back(1);`
- **THEN** vector 内部 buffer 从 bump arena 分配，`alloc.GetUsedBytes()` 相应增加

### Requirement: 内存统计

`FrameAllocator` SHALL 维护原子计数统计，零 per-allocation 元数据。

- `GetCurrentBytes() -> size_t` — 当前已分配字节数
- `GetPeakBytes() -> size_t` — 历史峰值字节数
- `GetAllocationCount() -> size_t` — 当前累计分配次数

`Allocate` 更新 `mCurrentBytes`/`mPeakBytes`/`mAllocationCount`；`Rewind` 回退 `mCurrentBytes`；`Reset` 清零 `mCurrentBytes` 并回退 `mPeakBytes`。

#### Scenario: 统计正确反映分配
- **WHEN** 连续 `Allocate(100)` 三次后 `GetCurrentBytes() == 300`；`Rewind` 回退一次后 `GetCurrentBytes() == 200`；`Reset` 后 `GetCurrentBytes() == 0`
- **THEN** 统计值与预期一致

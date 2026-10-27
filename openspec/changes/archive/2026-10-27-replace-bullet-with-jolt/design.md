## Context

本 change 将 Bullet **直接替换**为 Jolt，最终只有一个物理提供方。检查日期：2026-10-26。

当前实现有三层内容集中在 `plugins/bullet`：`BulletBackendWorld` 包装 SDK；`PhysicsSystem` / `PhysicsBodyComponent` / config 负责 Framework 对接；独立 render module 注册 Aurora mesh 转换。`engine/physics` 已有后端中立接口和稳定句柄，terrain/navigation 消费这些接口，不需要改成依赖 Jolt。

当前实际缺口影响迁移验收：

| 检查点 | 当前行为 | 迁移决策 |
|---|---|---|
| `BulletBackend.cpp` 世界构造 | 经典单线程世界，Fast | Jolt job update；保留 Fast，Exact 仍拒绝 |
| `CreateConstraint` | Distance 使用点约束，limits/motor 未应用 | 按契约实现距离、自由度、限位和明确的 motor 模式 |
| `ApplyFilter` / body 创建 | 更新 filter 没有更新 SDK，创建截断至 16 bits | 保留完整 32-bit group/mask，并使运行时修改生效 |
| `ApplyMaterial` | 只应用 dynamicFriction，combine 未实现 | 明确单摩擦系数限制，落实 combine，拒绝不支持的双系数语义 |
| `PhysicsSystem::SingleStep` | 跳过 Tick 的事件/插值完成逻辑 | 共用推进与完成路径 |
| module config factory | `(void)config` | 校验并映射配置，不能接受后忽略 |
| `PhysicsBodyComponent::GetPhysicsWorld` | 下转型具体 `PhysicsSystem` | 通过 `PHYSICS_SYSTEM_NAME` 的既有 `IPhysicsSystem` 契约访问 |
| Shape reflection | 缺 samples/points/children | 补齐 round-trip，保留类型名和已有字段 |

现有用户未提交的软体、飞行、PCG、Aurora 和 OpenSpec 整理工作不属于本次实现所有权；只在有明确迁移影响的当前说明中做局部补充。

## Goals / Non-Goals

**Goals:**

- 删除 Bullet 的可编译实现、依赖、开关、模块入口和测试；Jolt 成为默认且唯一交付后端。
- 保留物理消费接口、已有组件类型身份、World 子系统名和资产数据。
- 覆盖现有 shape/body/constraint/character/query/event/material/debug/snapshot 功能，并将 SDK 显式失败暴露给调用方。
- 在既有 VS 2022 build cache 完成相关构建、契约测试、模块生命周期和编辑器/游戏模板验证。

**Non-Goals:**

- 不保留双后端、Bullet forwarding headers、旧模块名 alias 或运行时切换菜单。
- 不承诺与 Bullet 逐帧一致，不以引擎更换直接宣称性能提升。
- 本次不交付软体、车辆、ragdoll authoring、GPU 物理、全引擎双精度或跨平台 Exact/rollback replay。
- 不重排 Aurora、terrain/navigation 或其他 plugin；不删除历史 archive、对比资料中的 Bullet 事实，也不删除工作区外共享三方缓存。

## Decisions

### 1. 目录、target 和装载入口

`engine/physics` 继续是接口/数据/纯工具层。原 Bullet plugin 中的后端无关运行时实现仍放在新 plugin，保留 `sky::phy` 命名空间，不把业务实现移进 engine，也不另建通用 physics runtime plugin。

| 位置 | Target | 职责及依赖 |
|---|---|---|
| `engine/physics` | `Physics`（保留） | 契约、数据、校验；不 include/link Jolt plugin |
| `plugins/jolt/adaptor` | `JoltPhysicsModule` | Jolt SDK backend/world/shape/query/contact/job、World 子系统、组件、config、模块入口和唯一 metadata owner；依赖 `Physics`、`Framework`，私有链接 `3rdParty::jolt` |
| `plugins/jolt/render` | `JoltPhysicsRenderModule` | 复用现有 Aurora mesh 转换及 provider 注册；依赖 `Physics`、`Framework`、`Aurora.Adaptor.Static`，不链接 Jolt SDK |
| `plugins/jolt/test` | `JoltPhysicsTest` | 直接编译 adaptor 实现的契约/集成测试；独立 executable，元数据由 main/Environment 一次注册，不为测试新增生产库 |

`JoltPhysicsModule` 本身就是 Framework adaptor 的动态装载入口，不再建立反向依赖 adaptor 的 runtime target。SDK 头和 `JPH::*` 类型仅存在于 `adaptor/src/backend` 私有头/源码。Jolt plugin 本身就是物理接口到第三方 SDK 的适配，没有独立于 SDK 的业务核心，因此不设 core 或额外静态后端 target；backend 文件按职责分组，不形成新的模块边界。跨模块消费者仍只 include `physics/...`；plugin 自身的对外组件头使用 `jolt/...`，不提供 `bullet/...` shim。

替换关系：`SKY_BUILD_BULLET -> SKY_BUILD_JOLT`、`bullet -> jolt`、`BulletPhysicsModule -> JoltPhysicsModule`、`BulletPhysicsRenderModule -> JoltPhysicsRenderModule`、`BulletPhysicsTest -> JoltPhysicsTest`。`plugins/plugins.json` 仍默认启用物理；生成的 module 清单和游戏模板只引用新名字。旧选项不自动映射，新构建/打包配置必须更新。

**方案确认：**用户于 2026-10-26 确认迁移，于 2026-10-27 明确要求简化为 adaptor + render，并授权检查、重新设计后继续；本表已按该指示更新。此次搬迁仅用于用户明确要求的后端替换；不扩大为全仓库目录整理。提交和归档仍需单独确认。

### 2. SDK 固定版本与构建

采用官方 [Jolt v5.6.0](https://github.com/jrouwe/JoltPhysics/releases/tag/v5.6.0)，完整 revision 为 `e77f175595e64cb44218cc9d9d56fc365ad0e36a`，已通过官方仓库 `git ls-remote` 核实。使用现有 `cmake/thirdparty.json` 的 tag/commit 和 `source: Build`，不使用浮动 master、FetchContent 或源码 vendoring。

核心构建选择：静态库、float、关闭 C++ exceptions/RTTI、关闭 IPO、Windows 使用 DLL CRT、关闭样例/测试/viewer、关闭 GPU compute 后端。本 change 先采用 SSE2 基线，关闭额外 x86 指令集选项；ARM 使用该版本支持的目标设置，不把 x86 参数硬塞给 ARM。`CROSS_PLATFORM_DETERMINISTIC` 默认关闭，不改变现有 Fast 产品承诺。

`Findjolt.cmake` 沿用 `sky_3rd_static` 的 Debug/Release 路径约定；所有影响 ABI 的定义必须与已安装库一致，包括 assertions、profiler、debug renderer、ObjectLayer 宽度和 SIMD。实现时对照固定 revision 的 [Build/CMakeLists.txt](https://github.com/jrouwe/JoltPhysics/blob/v5.6.0/Build/CMakeLists.txt) 及安装 target 导出，记录实际值，不能仅链接 `Jolt.lib` 后依赖头文件默认宏。保留 MIT notice。

沿用现有 `3RD_PATH`、generator 和第三方输出布局。Jolt 的引入只替换 bullet3 包项及 importer，不借机重写 bootstrap 的遗留 Python 异常路径。

### 3. 全局状态、世界和代码生命周期

- `PhysicsBackendRegistry` 保持 `Singleton<T>` 与单 active backend 契约；SDK 全局状态由进程唯一 `JoltSdkState : Singleton<JoltSdkState>` 协调，不能以普通函数 static 实现。
- 后端显式 `Init` 执行 allocator/Factory/type 注册，失败返回状态；形状、body、world 构造函数不承担可恢复初始化。Jolt 类型注册属于 SDK 生命周期，不等于 SkyEngine 元数据注册。
- 后端和每个世界持有 SDK state lease；registry 注销先撤销新建入口，已存在世界不失去 SDK 状态，最后一个 lease 释放后才执行对应 SDK 清理。
- 后端工厂、世界、持久回调同时保留 module code lease。复用 Framework 现有 definition lease / `WorldSubSystemRegistration::moduleLease`，对 `AttachPhysicsSystem` 手动附加路径也传递 lease；SDK lease 不能替代 DLL lease。
- 世界销毁顺序：阻止新操作、收齐 jobs、移除 listener、销毁约束/characters/bodies/shapes，随后销毁 native world、释放 SDK lease，最后释放代码 lease。不能仅靠裸 registry 当前 backend 去销毁旧世界。
- SkyEngine 反射只在 `JoltPhysicsModule::RegisterMeta` 的 definition 阶段注册；Init/Shutdown 只对称注册运行时服务。重启不会重新反射，也不加 once/查表跳过守卫。

### 4. 公共接口与失败语义

保留 `IPhysicsBackend` / `IPhysicsWorld` 和 `PhysicsObjectId`，新增引擎数据 `PhysicsStatus` / `PhysicsError`，覆盖 `InvalidArgument`、`Unsupported`、`CapacityExceeded`、`InvalidState`、`BackendFailure`。`Step` 与可能失败的 `SetOptions` 改为返回 `PhysicsStatus`，所有调用者、测试 fake 同批更新。创建函数仍可用 invalid handle/null 表达失败，并提供可读取的原因；原因不得只存在于临时日志。

Jolt `PhysicsSystem::Update` 返回 error flags，必须映射状态。收到失败后 world 进入 Faulted，jobs 仍全部完成；宿主停止后续推进并暴露原因，不把部分更新发布为成功帧，也不宣称失败 update 已自动回滚。恢复需显式重建世界。该步骤参考固定版本 [PhysicsSystem](https://github.com/jrouwe/JoltPhysics/blob/v5.6.0/Jolt/Physics/PhysicsSystem.h) 契约。

既有 `PhysicsOptions::maxBodies/maxActiveBodies=0` 仍表示“没有额外 gameplay 配额”，不能被误当成无限 native 容量。新增 `PhysicsWorldLimits`：body/character/job/contact pair/contact constraint 容量均有有限默认值，并在创建时确定。预算大于容量、运行时增加不可增长的 native 容量、非法配置都返回错误；已有对象不因失败的 SetOptions 被部分改变。

本轮所有 touched native failure 路径都检查显式返回值。不得新增 throw、try/catch、dynamic_cast，不以 assert/abort 处理业务容量或无效用户数据。

### 5. Body、shape 与空间约定

| 引擎数据 | Jolt 映射和规则 |
|---|---|
| Static / Dynamic / Kinematic | 映射 motion type；保持 origin transform 与 native center of mass 的区别，读取位姿返回引擎 origin |
| Box / Sphere / Capsule | capsule 输入 height 是圆柱段全高，转换为 native half-height；零高走等价球形 |
| ConvexHull | 验证有限坐标、至少四个非共面点；检查 native shape result |
| TriangleMesh | 私有烘焙；按 stride/U16/U32 边界读取并检查索引，禁止未对齐 reinterpret_cast |
| HeightField | 保持 tile-center 水平原点，`h = sample * heightScale + heightOffset`；矩形或不满足 native 网格约束的描述烘焙为等价静态 mesh，不截断/补造地形 |
| Compound / pivot | 递归组合，每层 pivot 只施加一次；任一 child 失败则整形状失败，释放已创建 child |
| scale | 校验有限、正 scale；烘焙/包装支持的 scale；SDK 不支持的组合显式拒绝，不能悄悄忽略 |
| force / impulse | `atWorldPos` 保持世界空间作用点语义，不当作质心偏移 |
| CCD | body 请求与 world gate 同时开启；仅接受本轮支持的动态 convex 组合 |

HeightField 的 X/Z 位置以 `(cols-1)*scaleX/2`、`(rows-1)*scaleZ/2` 居中，upAxis 转换保持右手系。`minHeight/maxHeight` 用于范围校验/烘焙，不再隐式平移垂直原点；terrain 的 tile transform 不需补偿 native shape 内部中心。用非零高程、非方形格网和相邻 tile seam 测试固定此规则。

动态 triangle mesh 在当前实现中并没有安全可用的通用求解契约。本轮拒绝 `allowDynamicTriangleMesh=true`，明确提示改用凸包/compound；不自动执行不可控的凸分解。shape cooking 只使用已解析 CPU 数据，不能在 worker/query/contact callback 中同步读取资产。

引擎句柄继续用 slot/generation 映射 native BodyID，后者不作为跨模块 ABI。显式区分 body/character/constraint/material，验证句柄后才能访问 native 对象。材料也检查 generation，避免旧材料 ID 指向复用 slot。

### 6. Filter、query、event 和 material

Broadphase 采用少量静态/运动类别；完整 `uint32_t group/mask` 存在 world 自有 payload，通过 pair/contact 过滤保证双方 mask 都接受，不把 32-bit mask 当 native ObjectLayer。修改 filter 要使既有接触缓存重新验证并唤醒受影响对象。query 同样应用完整 mask、ignore 和 ignoreExtra。

Raycast / Sweep / Overlap 使用 native narrow-phase query，输出仍为引擎 handle。closest hit 用距离再用稳定 handle 消除并列歧义；Overlap 去重并排序。Sweep 返回原点沿移动方向的距离，命中点/法线使用世界坐标。无效/非有限输入不进入 SDK。

Contact listener 可能来自 worker：只记录世界自有的接触状态，不调用 gameplay/反射/World API，也不从已移除 BodyID 解引用对象。维护 subshape pair 到 object pair 的引用计数，避免 compound 多接触导致重复 Begin/End；sleep 不制造伪 End。主线程在 jobs 完成后按稳定 ID 排序，生成 Begin/Stay/End 和 TriggerEnter/Exit；删除或改 filter 也按规定结束旧 pair。公开 payload 只填可可靠获得的量，不伪造 impulse。

material 保留既有字段和 combine enum。不同 combine 模式的优先级明确为 `Max > Multiply > Min > Average`，同一模式按其数学规则组合；在 native contact settings 设置有效摩擦与 restitution。Jolt 的基线接触只有一个摩擦系数，本轮仅接受 `staticFriction == dynamicFriction`，不承诺独立静/动摩擦；不同值返回 Unsupported，不能悄悄丢弃其中一个。descriptor 的方向阻尼仅接受 isotropic XYZ，相同值映射 native scalar；不等值显式拒绝。body 的非零 isotropic 阻尼覆盖对应 material 阻尼，零向量表示使用 material 阻尼；之后换 material 不覆盖已指定的 body 阻尼。

### 7. Constraint 和 character

约束建立前先解析两侧句柄。只有**显式 invalid** 的一侧代表 world anchor；stale/non-body ID 返回失败，不能被转换为 world anchor。两个 world anchor、自连接和无效轴拒绝。frame 使用 body-origin local space，native COM 偏移由后端转换。

| 类型 | 确定语义 |
|---|---|
| Fixed | 固定两个完整 frame |
| Hinge | 指定轴为旋转轴，使用 angularX 限位和 angular motor |
| Slider | 指定轴为平移轴，使用 linearX 限位和 linear motor |
| Distance | 两个 anchor 距离为 `distance`；零距离使用等价点约束 |
| Generic6DOF | 对应 local frame 的 XYZ 线性/角自由度；limited=false 表示自由，lower==upper 表示锁定 |

新增 motor mode `Velocity | Position`（默认 Velocity），消除同时存在 targetVelocity/targetPosition 的歧义；Hinge/Slider 使用对应 motor，Generic6DOF 现有单 motor 描述只绑定 local X，其余轴不假装存在独立 drive。其他类型上非默认、无法实现的限位/drive 返回 Unsupported。`collideConnected=false` 只过滤指定 body pair，不能屏蔽整个连通约束链。删除 body 先删除依赖 constraint。

character 采用 `CharacterVirtual`，每次固定步消费一次累计 displacement；其速度由 fixedDelta 转换，重力沿 `up`，stepHeight/slopeLimit 进入 native设置。即使没有移动输入也进行落地更新。可查询的 native inner/proxy body 与 character handle 同步；同一 character 的 proxy 不再作为独立 body 对外计数或产生重复命中。显式处理 character-character 交互，不默认只与刚体相撞。改变 capsule 保持 handle；空间不足或 cooking 失败时保留旧 capsule/transform。

### 8. 配置、推进、快照和调试

`PhysicsSubSystemConfig` 保留名字与旧字段，增加容量/worker 设置。gravity、fixedTimestep、maxSubSteps、solverIterations、sleepThreshold、continuousCollision 映射到明确的 world/step/native设置；collisionMargin 作为 convex radius 的请求上限，按形状尺寸合法裁剪。非默认 `deterministic=true` 在本轮返回 Unsupported。配置面板仍由通用反射 Bind，新增字段使用 CommonProperty 元数据。

一次完成路径：收集/校验输入 -> character 和 native fixed update -> 等待所有 jobs -> 检查状态 -> 同步当前位姿/统计 -> 整理事件 -> 更新插值。Tick 循环调用该路径，SingleStep 调用一次；暂停不累计隐藏时间。`jobStepping` 只在实际接通的 native job seam 上报告，workerCount=0 提供单线程实现；不另外创建不可控的每世界线程池，SDK job executor 由 backend session 统一持有并有界配置。

Snapshot 继续是 Fast 状态快照，恢复位姿、速度、睡眠和帧号；先校验所有 ID/数据再修改，恢复后重置接触缓存、事件和插值。它不包含完整求解缓存，不承诺跨平台或精确重演。debug 保持 `PhysicsDebugGeometry`，转换发生在模拟同步点，不直接调用 Aurora；stats 的 timing 不参与模拟输入。

### 9. 数据兼容与清理范围

保留 `PhysicsBody` component 注册名、`sky::phy::*` 类型身份、已有 enum 数值、字段名及 `Physics` 子系统名；增加缺失的 shape vector reflection。新字段有显式缺省值。不能为了更换 plugin 名重新生成类型 UUID。serialized 数据不写 `JPH::*` 或 native pointer。

| 删除/替换 | 操作 |
|---|---|
| `plugins/bullet/**` | 复用后端无关内容后完整删除；原测试移植到 Jolt 并扩展契约用例 |
| `cmake/thirdparty.json` / `Findbullet3.cmake` | 移除 bullet3，新增固定 jolt 包及 `Findjolt.cmake` |
| `plugins/plugins.json` / plugin manifest | 唯一启用 Jolt，去掉旧开关与旧 target |
| 游戏模板 / package smoke | 新 module 名称和 SKY_BUILD_JOLT；physics-disabled 仍可构建 |
| `PhysicsMesh.h` / helper 示例 / 当前模块说明 | 更新实现归属和构建命令；不改变 engine 的依赖方向 |
| 当前软体计划等 | 增补“本 change 选择 Jolt”的状态，保留此前对比结论的时间和上下文 |
| 当前输出内旧 Bullet DLL/PDB/模块清单 | 确认路径及生成属性后逐项删除或重生成，检查打包输入不再带旧文件 |
| 历史 archive / 第三方共享目录 | 保留历史证据和其他工作区可能使用的缓存；不纳入当前可构建依赖 |

清理验收以“源码、配置、依赖图、运行时装载、打包产物没有 Bullet”判定；全仓文本搜到历史分析并不代表失败。禁止批量替换历史诊断，把曾经发生的 Bullet ABI 故障改写成 Jolt 故障。

## Risks / Trade-offs

- [求解响应变化] -> 在固定步长、质量和场景下做接触/约束/角色测试；公开标记 Distance、filter、material 等旧缺口修正，不保证旧调参手感完全相同。
- [SDK ABI 宏/CRT 不一致] -> 固定 revision 和选项，Debug/Release 均验证 importer；保留 build cache 的 generator/3RD_PATH。
- [容量耗尽时 native 已部分推进] -> 返回错误，收齐任务并 Fault 世界，不能继续作为成功模拟输出。
- [SDK 与 DLL 生命周期不同] -> 两类 lease 都验证；覆盖 registry 注销而 world 仍存活、world 销毁、runtime restart。
- [原场景缺失了以前未序列化的字段] -> 无法凭空恢复丢失数据；保持可读取既有字段，并保证迁移后新保存的数据完整。
- [旧自定义项目 module 名称] -> 在迁移说明列出替换表；仓库内模板全部更新，仓库外项目由用户明确选择后再修改。
- [跨平台尚未运行] -> Windows 本地验证与其他平台证据分开记录；不得把 Windows 编译成功说成 Android/iOS/macOS 全部已验证。

## Migration Plan

1. 确认本设计的 target/装载替换表；记录工作区原有修改，不执行 commit/archive。
2. 接入固定 Jolt third-party 包并验证 Debug/Release ABI，保留当前 build cache。
3. 增加必要的引擎状态/容量/motor 数据契约，替换 backend 和所有消费调用点。
4. 迁移 adaptor、metadata 与 render bridge，保持序列化身份和正确代码 lease。
5. 更新默认配置、模板、打包与测试；删除 Bullet 实现/依赖和当前生成的旧模块产物。
6. 关闭编辑器 -> 第三方版本预检 -> 配置 -> 构建相关 targets -> 运行契约/集成测试 -> 有界编辑器和游戏模板 smoke。
7. 完成第三方/源码扫描、当前文档更新和 verification.md；一份 change 同时包含 spec 与实现。只有用户随后确认才提交或归档。

需要回退时整体回退本次变更集并恢复对应依赖/模块配置，不把两个 provider 同时装入一个进程。未提交阶段不得使用会覆盖其他修改的全仓 reset。

## Validation Matrix

| 组 | 必须验证 |
|---|---|
| SDK/build | 固定 commit，Debug/Release，静态/CRT/ABI，physics ON/OFF，生成模块清单 |
| body/shape | 七类 shape，mass/inertia，origin/COM，pivot/scale，CCD，睡眠，非有限值，失败回收 |
| terrain/mesh | 非零高程、矩形网格、tile seam、不同 upAxis，U16/U32/stride/越界索引 |
| filter/query | 高 16 bits、双方 mask、运行时改 filter、ignore、compound 去重、稳定排序 |
| contacts/material | Begin/Stay/End、sleep、删除、trigger、combine、材料句柄复用、不支持参数拒绝 |
| constraints | 五种关节、Distance 非零长度、limits/motor 模式、world anchor、stale ID、删除依赖 |
| character | 落地、坡度、台阶、移动平台、character-character、单步位移只消费一次、resize 失败保持原状态 |
| host/state | Tick/SingleStep、pause、worker/single-thread 同场景、snapshot、Faulted 不继续推进、容量耗尽 |
| lifecycle/data | Init 失败重试、注销后 world 存活、runtime restart、唯一 reflection、旧组件身份/数据 round-trip |
| cleanup | 相关源码/构建依赖无 Bullet；运行/打包不装载旧 DLL；历史记录保持原事实 |

## Open Questions

- **已确认：**第 1 节 target/归属/装载方案，以及本 change 中必要的 Bullet 目录替换。
- **实施验证项：**固定版本 ABI 定义、terrain/native形状边界和角色 proxy/接触行为需通过本次测试落证；未通过前不标记迁移完成。这些不改变已选的单后端方向。

## Implementation refinements

- User-directed layout revision: only adaptor + render production targets; tests compile adaptor sources directly.
- Heightfields use a triangle-grid cook for every grid size in this migration, preserving rectangular grids, signed heightScale/offset, upAxis and seams without native height quantization. This costs more memory than a native heightfield; native compression is a future optimization.
- The pinned SDK patch removes only the final Debug assertion on an already-returned EPhysicsUpdateError, after jobs and locks are released. Engine Step reports capacity failure and faults the world. Internal Jolt invariant assertions remain enabled in Debug. The patch also installs the MIT license with SDK headers.
- Shape collection round trips exposed the existing math JSON loader assuming a flattened named array. It now enters/exits an array node when loading a vector as a collection element; named vector field encoding stays unchanged.
- PhysicsMesh provider state uses Environment-backed Singleton storage and code leases so render registration is visible across DLLs.

## Integration contract corrections (2026-10-27)

The strict review exposed inherited boundary assumptions, not a need for another Jolt core layer. The production layout remains adaptor + render.

- **World construction:** `LoadJson` prepares detached, typed actors and resolves their hierarchy. `Build` is one-time initialization: validate every enabled subsystem and component requirement, construct candidates, attach subsystems, then attach actors. Missing registrations, invalid configuration, failed factories or failed component attachment return a reason and prevent publication by WorldDocument/PlaySession. New attachments are rolled back; a failed candidate can be corrected and retried. Registry names reject duplicate owners; Jolt unregisters only a registration it successfully acquired. Callback destruction precedes release of its DLL lease.
- **Authoring storage and application point:** authoring WorldDesc stays the source of truth. Configuration and enable edits apply to the next Play world, not by pretending that append-only Build performs hot reload. Configuration commands resolve document lifetime + subsystem name + reflected type at execution/undo time; vector growth cannot invalidate their targets. Reload expires old commands. Structural revisions rebind the form without interrupting ordinary field editing. Checkbox edits are undoable and do not implicitly save or activate a subsystem merely because its row is displayed.
- **Typed creation:** ComponentFactory owns typed constructors and required subsystem names. Actor UUID loading cannot construct arbitrary reflected values as ComponentBase. The editor enables declared dependencies together with component/actor creation in one undo step. Existing animation-specific skeleton defaults remain in their existing authoring path; subsystem dependency selection no longer uses that branch.
- **Creator extension:** EditorActorCreation stores stable id, label/group, callback and module lease. The callback prepares a detached actor. Common commands own world insertion, selection, dirty state and undo; Sandbox actions follow registration revisions. Removed/replaced registrations cannot be invoked through stale menu callbacks, while existing undo commands retain their code.
- **Editing and stepping:** reflected bool setters propagate failure through PropertyEditCommand, ReflectedForm and CommandService. Jolt mass/shape edits cook before mutation and update the existing native body; handle, velocity and connected constraints survive, including COM corrections. Unsupported motion transitions are rejected. Transform events supply static/dynamic teleports and kinematic targets; World runs actor input, subsystem simulation, then component PostTick for dynamic interpolation output, with a feedback guard.
- **Mesh and asset boundary:** the render bridge reads the actual POSITION format/offset, vertex stride and U16/U32 index declaration from an Aurora product, validates ranges and compacts position data. Tests use MeshAssembler, MeshAssetWriter, binary header round-trip and bulk reads. Collision UUIDs, including compound children, become typed World assetRoots; an unknown product type is rejected during package root collection. The real product-only auditor checks missing mesh products.
- **Event ownership:** listener dispatch sees the complete ordered batch for each host Tick/SingleStep. Optional polling exposes that same current batch and must drain it before the next host tick; no historical backlog is retained. Begin/End delivery to listeners remains intact. Backend standalone DrainEvents ownership is unchanged.

These are changes to existing contracts, registries and commands. No module relocation, independent Jolt core, replacement reflection system or second actor creation registry was introduced. Component attachment hooks must not mutate World actor membership; fallible hooks must release partial work through OnDetachFromWorld. Generic command groups rely on completed commands remaining reversible during the synchronous group operation.

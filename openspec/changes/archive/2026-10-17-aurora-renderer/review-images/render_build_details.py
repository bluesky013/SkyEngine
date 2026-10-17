"""ECS, FrameGraph and resource state diagrams for the detailed design."""
from render_diagrams import Diagram, MUTED


def ecs_build():
    d = Diagram(1110, '06  ECS 构建：持久状态 + 每帧增量',
                '设计细化 · 主线程交付输入；coordinator 管结构；worker 只做获授权的数据计算')
    d.box((70, 175, 520, 285), 'World / Actor / Components', '主线程维护稳定代理映射', 'blue', 28)
    d.box((600, 175, 1530, 285), 'FramePacket：Create / Update / Destroy / CpuReady payload',
          '实体 generation + bindingRevision + 不可变载荷', 'blue', 27)
    d.arrow([(520, 230), (600, 230)])
    d.arrow([(1065, 285), (1065, 325)])
    d.box((600, 325, 1530, 430), 'ApplyCommands → Preflight → 冻结结构与资源版本',
          '预建池 / 派生组件 / queries；回退 mesh 与 metadata 在此选定', 'purple', 29)
    d.box((70, 325, 520, 715), 'SceneStore · 持久 ECS',
          'WorldInfo / RenderFlags\nMeshBinding / MaterialBinding\nLight\nBounds + 输入版本 stamp\n\nGPU 指针与 RDG handle 不入池', 'white', 29)
    d.arrow([(600, 380), (520, 380)])
    d.arrow([(1065, 430), (1065, 485)])
    d.box((600, 485, 1530, 595), 'Scene prepare DAG：Bounds / Lights / Objects',
          'Read/Write 依赖决定先后；可拆分 kernel 使用 chunks 与私有 scratch', 'purple', 28)
    d.arrow([(1065, 595), (1065, 650)])
    d.box((600, 650, 1530, 740), 'SceneReady → 多 View 的 Cull / LOD / Extract',
          '只读同一 Scene；写各 View 自有输出', 'purple', 28)
    d.arrow([(1065, 740), (1065, 780)])
    d.box((600, 780, 1530, 885), 'PreparedFrame / CpuDrawPackets',
          '精确 mesh/material 版本 + submesh + object data + 排序键', 'green', 29)
    d.box((70, 780, 520, 885), '下一轮修改的门槛', '本轮 jobs 与 ECS 借用均已退出', 'amber', 28)
    d.arrow([(1065, 885), (1065, 925)])
    d.box((600, 925, 1530, 995), '交给 ResourceFinalize 与 FrameGraphBuilder', color='green', size=29)
    d.footer(1020, ['Dirty stamp 用来跳过重复计算，不替代读写依赖；Scene ECS 不逐帧重建。',
                    '晚期上传未获准时跳过或重新 prepare；不能偷换几何版本后沿用不匹配的 Bounds。'])
    d.save('06-ecs-build.png')


def framegraph_build():
    d = Diagram(1340, '07  FrameGraph 构建：输入、Pass 与编译',
                '一个 FrameGraph 使用现有 RDG · 图内箭头表示数据依赖，不代表多队列并行')
    d.box((65, 175, 535, 280), 'PreparedFrame', 'Scene/View 结果 + UI 快照', 'purple', 29)
    d.box((565, 175, 1035, 280), '有效 OutputLeases', '实际 extent / format / generation', 'blue', 29)
    d.box((1065, 175, 1535, 280), 'ResourceResolution', 'Resident / InGraph / Unavailable', 'amber', 29)
    for x in (300, 800, 1300):
        d.arrow([(x, 280), (x, 305), (800, 305), (800, 335)])
    d.box((250, 335, 1350, 420), 'ImportTable 去重 / transient 声明 / UploadBatch',
          'coordinator 串行构建；graph handles 只在本图有效', 'blue', 29)
    d.arrow([(800, 420), (800, 455)])
    d.box((430, 455, 1170, 545), '共享 Upload passes', 'staging READ → mesh / material / scene data WRITE', 'green', 29)
    for x in (410, 1190):
        d.arrow([(800, 545), (800, 570), (x, 570), (x, 600)])
    d.box((120, 600, 700, 700), 'ScenePass(A) → ColorA / DepthA', 'View A 独立 camera / bindings', 'green', 27)
    d.box((900, 600, 1480, 700), 'ScenePass(B) → ColorB / DepthB', 'View B 独立 camera / bindings', 'green', 27)
    d.arrow([(410, 700), (410, 735)])
    d.arrow([(1190, 700), (1190, 735)])
    d.box((120, 735, 700, 825), 'UI 合成 → 编辑器主窗', '显式 READ ColorA', 'green', 29)
    d.box((900, 735, 1480, 825), 'Composite → 第二窗口', '显式 READ ColorB', 'green', 29)
    d.arrow([(410, 825), (410, 860)])
    d.arrow([(1190, 825), (1190, 860)])
    d.box((120, 860, 700, 930), 'Root：Present 主窗', color='amber', size=28)
    d.box((900, 860, 1480, 930), 'Root：Present 第二窗口', color='amber', size=28)
    d.text((800, 965), '根还包括：持久 export / 获准上传 / 明确副作用；普通 Import 不自动成为根',
           25, MUTED, anchor='mt')
    d.box((65, 1030, 1535, 1125), '读取版本 → 内容剔除 → Hazard / 拓扑 → Lifetime / Backing → Barrier',
          'Compile 成功后：ResolveBindings → Record → Submit → 发布提交记录', 'blue', 27)
    d.footer(1170, ['每个 Read 绑定当时的 producer/version；UI LOAD 必须保留底图生产者。',
                    '一个输出失效只去掉其无用分支；另一个输出仍需要的共享上传保留。',
                    'CopyBlit 必须真正发出 copy；Compile/Record 不直接发布“资源上传成功”。'])
    d.save('07-framegraph-build.png')


def resource_async():
    d = Diagram(1320, '08  资源异步：请求、图内使用与全局就绪',
                '首版默认同 graphics queue 上传 · GPU 创建仍由 coordinator 执行 · 不引入协程 runtime')
    d.box((65, 175, 505, 300), 'Requested → CpuPreparing', '资产 executor：读取 / 解码\n完成结果回主线程归并', 'blue', 27)
    d.box((580, 175, 1020, 300), 'CpuReady payload', 'immutable metadata + bytes\n经 FramePacket 交付', 'blue', 28)
    d.box((1095, 175, 1535, 300), 'ResourceTicket 去重', 'ResourceKey + content revision\nsubscriber / device epoch 校验', 'purple', 28)
    d.arrow([(505, 235), (580, 235)])
    d.arrow([(1020, 235), (1095, 235)])
    d.arrow([(1315, 300), (1315, 345)])
    d.box((1095, 345, 1535, 475), 'UploadQueued', '预算允许才创建 backing\n分配 slot-owned staging', 'purple', 29)
    d.arrow([(1095, 410), (1020, 410)])
    d.box((580, 345, 1020, 475), '本图上传接纳检查', '接纳后：graph-local InGraph\n只对这个 GraphId 有效', 'amber', 27)
    d.arrow([(580, 410), (505, 410)])
    d.box((65, 345, 505, 475), 'Unavailable', '未被接纳 / 资源不完整\n跳过或已准备好的回退', 'white', 28)
    d.text((540, 370), '未接纳', 20, MUTED, anchor='mt')
    d.arrow([(800, 475), (800, 520)])
    d.box((580, 520, 1020, 640), '声明 RDG：Copy → Draw', '声明读写 + barrier\nCPU 不先等 GPU 上传', 'green', 27)
    d.arrow([(800, 640), (800, 680)])
    d.box((580, 680, 1020, 790), 'Compile / Record 成功', '只生成 tentative receipts', 'purple', 28)
    d.arrow([(800, 790), (800, 830)])
    d.box((580, 830, 1020, 950), 'Submit Accepted', 'ticket → UploadSubmitted\nslot 保活 staging / backing', 'purple', 29)
    d.arrow([(1020, 890), (1095, 890)])
    d.box((1095, 830, 1535, 950), 'GPU Copy / Draw 完成', '必要范围已初始化\n状态 / ownership 已知', 'green', 28)
    d.arrow([(1315, 950), (1315, 990)])
    d.box((1095, 990, 1535, 1095), 'Resident(version)', '后续图可导入此精确版本', 'green', 30)
    d.box((65, 550, 505, 790), 'Failed / CancelRequested', '未提交：回滚候选\n已提交：停止发布，仍需退休\n共享 ticket 按订阅取消\n未知提交错误：隔离状态', 'amber', 26)
    d.box((65, 830, 505, 1095), '热更新保留两份版本', 'desired revision ≠ 当前绑定\n本帧 pin 精确旧/新版本\n旧 GPU 引用全部退休后释放\n不原地覆盖在途 descriptor', 'white', 28)
    d.box((580, 990, 1020, 1095), '只有接受，尚未完成', '此时不能宣称全局 Resident', 'amber', 28)
    d.footer(1150, ['同图 InGraph 可以 Copy 后立即 Draw；首版其它图等待 Resident 或使用预先选定的回退。',
                    'CPU 源 bytes：全部读取/拷贝结束；staging：copy 完成；backing：所有消费者退休。',
                    '无需窗口也能提交获准的持久上传；资源未就绪不阻塞整个 SceneReady。'])
    d.save('08-resource-async.png')


def framecontext():
    d = Diagram(1490, '09  FrameContext 接入：一个 GPU slot 管理入口',
                '设计版 · 复用现有 DeviceFrameContext · FrameScheduler 负责调度，不另建 GPU ring')
    d.box((180, 175, 1420, 270), 'RendererImpl',
          '初始化 DeviceFrameContext；协调 CPU prepare、建图、提交与完成发布', 'blue', 32)
    for x in (285, 775, 1290):
        d.arrow([(800, 270), (800, 295), (x, 295), (x, 325)])
    d.box((65, 325, 505, 655), 'CpuPrepareFrame',
          '专用 CPU ThreadPool\nDeviceFrameDispatcher / JobGroup\ntask / chunk 私有 scratch\nPreparedScene / PreparedViews\n\n不占 GPU slot 即可执行', 'purple', 29)
    d.box((555, 325, 995, 655), 'FrameScheduler',
          '选择 packet 与 outputs\n请求 slot / 声明逻辑图\n决定提交与错误收尾\n持业务 SubmissionRecord\n\n不复制 native slot 状态', 'white', 29)
    d.box((1045, 325, 1535, 655), 'DeviceFrameContext',
          '唯一 GPU FrameSlot ring\n后端 Compiler / Dispatch\ncommand 资源 / reset 域\ndescriptor / staging / UBO 分区\n\n后端编码 ThreadContext', 'green', 29)
    for x in (285, 775, 1290):
        d.arrow([(x, 655), (x, 695), (800, 695), (800, 730)])
    d.box((65, 730, 1535, 835), 'FrameBuildContext · 本次建图的临时组合',
          'PreparedFrame + DeviceFrameLease + OutputLeases + GraphBuildArena', 'blue', 30)
    d.arrow([(800, 835), (800, 865), (530, 865), (530, 900)])
    d.box((65, 900, 995, 995), '后端 Compile → Context Dispatch → 收齐编码结果',
          'native pass / 编码任务计划；所有资源来自同一个 lease', 'blue', 27)
    d.box((1045, 900, 1535, 1130), '一个 slot · 多 View 切片',
          'View A：camera / object 常量\nView B：camera / object 常量\n共享 upload staging\n后端选择串行 / 并行编码', 'green', 28)
    d.arrow([(530, 995), (530, 1030)])
    d.box((65, 1030, 995, 1130), 'DeviceFrameContext.Submit(lease)',
          '紧邻提交 Reset fence → Queue::Submit → 登记接受/拒绝/错误', 'green', 29)
    d.arrow([(530, 1130), (530, 1170)])
    d.box((65, 1170, 995, 1265), 'PollCompleted → GPU slot 退休 / renderer 发布资源状态',
          '只有真实完成才能复用；未知提交错误隔离处理', 'green', 27)
    d.box((1045, 1170, 1535, 1265), 'Presentation 独立退休',
          '窗口关闭不只看 graphics fence', 'amber', 28)
    d.footer(1320, ['CPU arena：jobs / borrowers / 录制回调退出后回收；GPU slot：提交完成后复用。',
                    'FrameContext.parallelNum 管 GPU 编码；CPU prepare 的 worker 数独立配置。',
                    '只 reset 已完成 slot 的 command 资源；不能连带 reset 其它在途 slot 的 pool / allocator。'])
    d.save('09-framecontext.png')


if __name__ == '__main__':
    ecs_build()
    framegraph_build()
    resource_async()
    framecontext()
    print('Wrote detailed design figures 06-09')

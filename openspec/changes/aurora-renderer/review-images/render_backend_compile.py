"""Backend-owned compilation and encoding diagrams; design artifacts only."""
from render_diagrams import Diagram, MUTED


def backend_compile():
    d = Diagram(1600, '10  后端 Compile → FrameContext Dispatch',
                '设计版 · 运行时仅选择一个后端 · 公共声明统一语义，后端决定 native pass 与编码任务')
    d.box((180, 165, 1420, 265), 'FrameComposer / Features → FrozenFrameGraph',
          'logical passes · versioned accesses · bindings · roots · 已获取的 outputs', 'blue', 31)
    d.arrow([(800, 265), (800, 305)])
    d.box((180, 305, 1420, 395), 'DeviceFrameContext.Compile(lease, graph)',
          '委托当前后端完整编译；可组合复用公共纯分析函数', 'blue', 30)
    xs = (300, 800, 1300)
    rects = ((65, 445, 535, 865), (565, 445, 1035, 865), (1065, 445, 1535, 865))
    for x in xs:
        d.arrow([(800, 395), (800, 420), (x, 420), (x, 445)])
    d.box(rects[0], 'Vulkan Compile',
          '内容分析 / 剔除 / GPU 顺序\nDynamic Rendering scopes\nbacking / bindings / barriers\nsecondary encode chunks\nprimary 组装计划\nworker × slot recording pools', 'purple', 30)
    d.box(rects[1], 'DX12 Compile',
          '内容分析 / 剔除 / GPU 顺序\nNative pass / OM+clear 方案\nbacking / bindings / states\nDIRECT list encode tasks\n有序 lists / barrier 计划\n独占 list / allocator', 'purple', 30)
    d.box(rects[2], 'Metal Compile',
          '内容分析 / 剔除 / GPU 顺序\nRender / Compute / Blit scopes\nbacking / bindings / 同步\nparallel parent / child tasks\n有序 children / encoder 边界\nworker autorelease contexts', 'purple', 30)
    for x in xs:
        d.arrow([(x, 865), (x, 900), (800, 900), (800, 935)])
    d.box((65, 935, 1535, 1045), 'BackendCompiledFrame · 后端私有编译产物',
          'NativePassPlans + EncodeTaskGraph + SubmissionPlan + ResourceLeases', 'green', 31)
    d.arrow([(800, 1045), (800, 1080)])
    d.box((65, 1080, 1535, 1210), '同一 DeviceFrameContext.Dispatch',
          '本后端 ThreadPool / ThreadContexts + DeviceFrameDispatcher worker 批次\ncoordinator 步骤 → 并行 Encode → drain → 后端组装', 'green', 31)
    d.arrow([(800, 1210), (800, 1245)])
    d.box((180, 1245, 1420, 1335), 'TryCollect → RecordedFrame → 显式 Submit',
          '只提交完整成功结果；编码完成 ≠ GPU 完成', 'blue', 31)
    d.arrow([(800, 1335), (800, 1370)])
    d.box((180, 1370, 1420, 1445), 'PollCompleted → slot 退休 / 资源 Resident 发布', color='green', size=29)
    d.footer(1480, ['一个 logical pass 可拆成多个编码任务；多个 logical passes 可合法合并为 native scope。',
                    'plan 绑定 backend / context / slot generation；单次 dispatch，失败收齐后才能回收。'])
    d.save('10-backend-compile.png')


def encode_graph():
    d = Diagram(1370, '11  GPU 执行依赖与 CPU 编码任务是两张图',
                '示例前提：backing、bindings、draw packets 已冻结；各编码单元使用独立资源')
    d.text((65, 165), 'GPU：保证数据可见性与正确消费顺序', 31, bold=True)
    d.box((65, 235, 420, 370), 'Upload', '写 mesh / texture', 'green', 31)
    d.box((565, 215, 1035, 300), 'Scene A → ColorA', color='green', size=29)
    d.box((565, 340, 1035, 425), 'Scene B → ColorB', color='green', size=29)
    d.box((1180, 235, 1535, 370), 'UI / Composite', '读取 ColorA / ColorB', 'green', 29)
    d.arrow([(420, 300), (490, 300), (490, 257), (565, 257)])
    d.arrow([(490, 300), (490, 382), (565, 382)])
    d.arrow([(1035, 257), (1105, 257), (1105, 300), (1180, 300)])
    d.arrow([(1035, 382), (1105, 382), (1105, 300)])
    d.text((800, 450), '由后端 GPU 顺序 + barriers / encoder 同步保证；CPU 完成次序不能替代它',
           25, MUTED, anchor='mt')
    d.text((65, 525), 'CPU：在满足录制前提后，消费者命令也可以提前编码', 31, bold=True)
    d.box((350, 590, 1250, 680), 'Publish Bindings / Native Setup',
          '不可变快照 · 资源保活 · 为任务分配独立 result slots', 'blue', 30)
    centers = (245, 615, 985, 1355)
    titles = ('Encode Upload', 'Encode Scene A', 'Encode Scene B', 'Encode UI')
    for x, title in zip(centers, titles):
        d.arrow([(800, 680), (800, 715), (x, 715), (x, 750)])
        d.box((x-175, 750, x+175, 850), title, '任务可并发完成', 'purple', 26)
        d.arrow([(x, 850), (x, 890), (800, 890), (800, 925)])
    d.box((180, 925, 1420, 1025), 'Drain / Assemble → RecordedFrame',
          '按计划身份取结果；不按 worker 完成先后重排命令', 'blue', 31)
    d.arrow([(800, 1025), (800, 1060)])
    d.box((180, 1060, 1420, 1145), 'GPU 执行：按 SubmissionPlan 与已编码同步消费资源', color='green', size=30)
    d.footer(1190, ['这不是固定的跨后端任务模板：每个后端依据 native 约束自行构图。',
                    'Vulkan：secondary 结果先收齐，再组装 primary；Metal：parent 创建 → children → parent End。',
                    'parallelNum=1 走同协议串行模式；加载完成 / 编码完成 / 提交接受 / GPU 就绪分开。'])
    d.save('11-encode-task-graph.png')


if __name__ == '__main__':
    backend_compile()
    encode_graph()
    print('Wrote backend compile figures 10-11')

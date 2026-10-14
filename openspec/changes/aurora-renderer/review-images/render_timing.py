"""Thread/GPU timing illustrations for the proposed Aurora renderer."""
from render_diagrams import Diagram, INK, MUTED


def pipeline_overlap():
    d = Diagram(1110, '04  主线程 / 渲染线程 / GPU · 帧间重叠',
                '建议执行模型 · 时间向右 → · 方块宽度仅示意，不代表实测耗时')
    xs = [240, 555, 870, 1185]
    d.draw.rounded_rectangle((852, 195, 1168, 920), radius=20,
                            fill='#EBF0F7', outline='#C5D3E0', width=2)
    d.text((1010, 210), '三者可同时推进', 25, MUTED, anchor='mt')
    for x in xs[1:]:
        for y in range(255, 910, 18):
            d.draw.line((x-18, y, x-18, y+7), fill='#CCD8E5', width=2)
    for y, label, sub in [(265, '主线程', 'World / UI'),
                          (505, '渲染线程', '协调与 GPU 录制'),
                          (745, 'GPU', 'Graphics Queue')]:
        d.text((65, y+24), label, 32, bold=True)
        d.text((65, y+75), sub, 20, MUTED)
    for i, x in enumerate(xs):
        d.box((x, 265, x+280, 395), f'CPU 帧 N{f"+{i}" if i else ""}',
              'World / UI 更新\n生成 FramePacket', 'blue', 29)
    for i, x in enumerate(xs[1:]):
        d.box((x, 505, x+280, 635), f'Render N{f"+{i}" if i else ""}',
              '协调 prepare\n建图 / 录制 / 提交', 'purple', 29)
        previous = xs[i] + 280
        d.arrow([(previous, 395), (previous, 440), (x+45, 440), (x+45, 505)])
        d.text((previous-30, 450), 'Packet 入队', 22, MUTED, anchor='rt')
    for i, x in enumerate(xs[2:]):
        d.box((x, 745, x+280, 130+745), f'GPU N{f"+{i}" if i else ""}',
              'Upload / Scene Draw\nUI / Composite', 'green', 28)
        previous = xs[i+1] + 280
        d.arrow([(previous, 635), (previous, 680), (x+45, 680), (x+45, 745)])
        d.text((previous-25, 690), 'Submit', 23, MUTED, anchor='rt')
    d.text((380, 560), '尚未取得 N', 25, MUTED, anchor='mt')
    d.text((525, 800), '尚未提交 N', 25, MUTED, anchor='mt')
    d.footer(960, ['主线程 N+2、渲染线程 N+1 与 GPU N 可以重叠；图中只是其中一种正常排布。',
                   'Packet 队列满会产生 CPU 背压；GPU slot 不可复用会限制渲染线程继续提交。',
                   '帧序号、GPU submission serial、FrameSlot index 与 swapchain image index 分开管理。'])
    d.save('04-thread-gpu-overlap.png')


def sequence():
    d = Diagram(1950, '05  一帧 N 的线程与 GPU 时序',
                '建议版 · 时间向下 ↓ · 单 graphics queue · 补充 prepare worker 泳道')
    main, render, workers, gpu = 230, 650, 1040, 1410
    lanes = [(main, 65, 395, '主线程', 'World / UI / 窗口', 'blue'),
             (render, 465, 835, '渲染线程', 'Renderer coordinator', 'purple'),
             (workers, 885, 1195, 'CPU worker 池', 'Scene / View prepare', 'purple'),
             (gpu, 1260, 1560, 'GPU', 'Graphics Queue', 'green')]
    for center, x0, x1, title, detail, color in lanes:
        d.box((x0, 175, x1, 270), title, detail, color, 30)
        for y in range(285, 1780, 20):
            d.draw.line((center, y, center, y+10), fill='#ABBACB', width=2)

    def message(x0, x1, y, label, dashed=False):
        d.arrow([(x0, y), (x1, y)], dashed=dashed)
        d.text(((x0+x1)/2, y-39), label, 23, MUTED, anchor='mt')

    d.box((65, 305, 395, 390), '更新 World / UI', '封装拥有数据的 Packet N', 'blue', 27)
    message(main, render, 445, 'TrySubmit(N) → 有界 FIFO')
    d.box((465, 485, 835, 580), 'ApplyCommands', '输出 preflight / 冻结结构', 'purple', 28)
    d.box((65, 500, 395, 650), '开始主线程 N+1', '继续 World / UI\n处理窗口消息', 'blue', 28)
    message(render, workers, 630, '派发访问依赖 DAG')
    d.box((465, 680, 835, 905), '等待本帧 JobGroup', '继续处理控制状态\n保持 ECS 结构冻结\n不应用下一 packet', 'amber', 28)
    d.box((885, 680, 1195, 780), 'Scene prepare', '无冲突 system / chunks', 'purple', 26)
    d.arrow([(workers, 780), (workers, 815)])
    d.box((885, 815, 1195, 915), 'SceneReady → Views', '多 View 裁剪 / LOD', 'purple', 25)
    d.box((65, 780, 395, 985), '后续包入队', '队列满返回 Busy\n保留原包并处理事件\n不丢 lifecycle 命令', 'blue', 28)
    message(workers, render, 980, '全部 jobs 退出 / 结果发布', True)

    # This particular example is GPU-limited: N needs a slot still used by N-1.
    d.box((1260, 330, 1560, 1120), '执行较早提交', '例如 GPU N−1\n\nUpload copy\nScene Draw\nUI / Composite\n\nCPU 与 GPU 重叠\n\n末尾 signal fence', 'green', 27)
    d.box((465, 1035, 835, 1125), '等待可复用 FrameSlot', '本例：所需 slot 仍在途', 'amber', 26)
    message(gpu, render, 1175, '旧 submission 已完成；coordinator 查询到后复用 slot', True)
    d.box((465, 1220, 835, 1320), '有界 Acquire / 版本复核', '输出变化：重做 View 或跳过', 'amber', 26)
    d.box((465, 1350, 835, 1455), 'ResourceFinalize / RDG', 'Compile / Bindings / Record', 'purple', 25)
    message(render, gpu, 1510, 'Submit(N) · Accepted 只表示已接受')
    d.box((465, 1550, 835, 1720), '提交后继续 CPU N+1', '按后端协议排入 Present\n释放已用完的 CPU 借用\n不固定逐帧等待 GPU', 'purple', 27)
    d.box((1260, 1550, 1560, 1720), '执行 GPU N', 'Upload → Scene\nUI → 输出最终状态\n末尾 signal fence', 'green', 27)
    message(gpu, render, 1770, '后续观察 GPU 完成 → 对应 slot / GPU lease 才可退休', True)
    d.footer(1820, ['等待 job、slot、Acquire 时仍需保证控制流程进展；R1–R4 的相关契约已纳入设计，尚未实施。',
                    'Scene 下一轮 CPU 写入前必须结束上一轮 jobs 与相关借用；GPU 使用由 slot 保活的数据。',
                    'Present 排队不代表画面已显示；GPU fence 也不能代替 presentation retirement。'])
    d.save('05-thread-gpu-sequence.png')


if __name__ == '__main__':
    pipeline_overlap()
    sequence()
    print('Wrote 04-thread-gpu-overlap.png and 05-thread-gpu-sequence.png')

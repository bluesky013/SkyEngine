## 1. RenderGeometry 落地

- [x] 1.1 `include/aurora/resource/RenderGeometry.h`：`class RenderGeometry : public RefObject`（header-only）
- [x] 1.2 成员：`name` / `std::vector<std::unique_ptr<VertexBuffer<>>> vertexStreams` / `std::unique_ptr<IndexBuffer<>> indexBuffer` / `AABB localBounds`
- [x] 1.3 `SetLocalBounds(AABB)` / `AddVertexStream(std::unique_ptr<VertexBuffer<>>)` / `SetIndexBuffer(std::unique_ptr<IndexBuffer<>>)` + `GetVertexStreams()` / `GetIndexBuffer()` / `GetLocalBounds()` / `GetName()` 访问器；删除拷贝构造/赋值

## 2. 测试

- [x] 2.1 `core/test/RenderGeometryTest.cpp`：`RenderGeometryCompositeMetadata`（AddVertexStream/SetIndexBuffer/SetLocalBounds + 访问器 + 无 Upload 断言）
- [x] 2.2 `RenderGeometryOwnsBuffers`（Vulkan）：geometry 析构后底层 buffer 释放（unique_ptr 所有权）

## 3. 验证与收尾

- [x] 3.1 `cmake --build` AuroraCore 通过
- [x] 3.2 `AuroraCoreTest` 全绿
- [ ] 3.3 `openspec archive aurora-render-geometry` 归档

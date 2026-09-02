## 1. 接口头文件

- [x] 1.1 `RenderGraph.h`：`Import` 签名改为 `const Name &name, const ImagePtr &image` / `const Name &name, const BufferPtr &buffer`；所有 `const std::string &name` 参数改为 `const Name &name`；新增 `#include <core/name/Name.h>`，`<string>` 若不再使用则移除
- [x] 1.2 `RDGGraph.h`：`GraphImportImage::image` 改为 `ImagePtr`，`GraphImportBuffer::buffer` 改为 `BufferPtr`；`ResourceNode::name` / `PassNode::name` 改为 `Name`
- [x] 1.3 `RDGContext.h`：`mPassName` 改为 `Name`；`SetPassName(const Name &)`；`GetPassName()` 返回 `std::string_view`

## 2. 实现

- [x] 2.1 `RenderGraph.cpp`：`AddResource` / `AddPass` / `CreateTexture` / `CreateBuffer` / `Import` / `AddRasterPass` / `AddComputePass` / `AddCopyPass` 签名更新为 `Name`；`Import` 内 `import.image = image` / `import.buffer = buffer`（拷贝 `CounterPtr`）
- [x] 2.2 `Compile.cpp`：`mResolvedImages` / `mResolvedBuffers` 填充 import 资源处改用 `mImportImages[...].image.Get()` / `mImportBuffers[...].buffer.Get()`
- [x] 2.3 `Execute.cpp`：`ctx.SetPassName(pass.name)`（去掉 `.c_str()`）；debug 日志如需输出名称，经 `Name` 的 `operator<<` 输出

## 3. 测试

- [x] 3.1 `RDGTest.cpp`：`Import` 调用改为传 `ImagePtr`/`BufferPtr`（去掉 `.Get()`）；字符串字面量 `"dead"`/`"live"`/`"backbuffer"` 改为 `Name("...")`；`"t" + std::to_string(i)` 改为 `Name(("t" + std::to_string(i)).c_str())`
- [x] 3.2 确认 `RDGHandleTest`（Equality / Hash）与 `static_assert` 类型安全检查仍编译通过

## 4. 验证与收尾

- [x] 4.1 全量 `cmake --build` 通过（至少 Windows/Vulkan）
- [x] 4.2 跑 `AuroraTest --gtest_filter=RDG*` 全绿，全集无回归
- [ ] 4.3 `openspec archive aurora-rdg-import-ptr-name` 归档本 change

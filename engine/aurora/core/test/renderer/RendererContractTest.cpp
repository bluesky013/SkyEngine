// Linking implementation libraries would invalidate this public-contract boundary test.
#include <aurora/rdg/DeviceFrameContext.h>
#include <aurora/rdg/FrozenFrameGraph.h>
#include <aurora/renderer/IRenderFeature.h>
#include <aurora/renderer/IRenderer.h>
#include <core/async/Completion.h>
#include <type_traits>

using namespace sky::aurora;

static_assert(!std::is_convertible_v<ViewId, ViewportId>);
static_assert(!std::is_convertible_v<SceneId, RenderEntityId>);
static_assert(!std::is_copy_constructible_v<FramePacket>);
static_assert(std::is_nothrow_move_constructible_v<FramePacket>);
static_assert(!std::is_copy_constructible_v<FrameCompileResult>);
static_assert(!std::is_copy_constructible_v<FrameCollectResult>);
static_assert(std::is_abstract_v<IRenderer>);
static_assert(std::is_abstract_v<IRendererProvider>);
static_assert(std::is_abstract_v<DeviceFrameContext>);
static_assert(std::is_abstract_v<FrameGraphDeclarations>);
static_assert(std::is_abstract_v<FrameGraphServices>);
static_assert(std::is_base_of_v<FrameGraphDeclarations, FrameGraphBuildContext>);
static_assert(std::is_base_of_v<FrameGraphServices, FrameGraphBuildContext>);
static_assert(std::is_const_v<std::remove_reference_t<decltype(std::declval<FrozenFrameGraph &>().GetData())>>);
static_assert(std::is_abstract_v<sky::ICompletion>);
static_assert(static_cast<uint8_t>(VulkanFrameGraphMode::DYNAMIC_RENDERING) == 0);
static_assert(static_cast<uint8_t>(VulkanFrameGraphMode::RENDER_PASS) == 1);
static_assert(static_cast<uint8_t>(RendererVulkanMode::DYNAMIC_RENDERING) == 0);
static_assert(static_cast<uint8_t>(RendererVulkanMode::RENDER_PASS) == 1);
static_assert(DeviceFrameContextInitInfo{}.vulkanCompiler == VulkanFrameGraphMode::AUTO);
static_assert(RendererInitInfo{}.vulkanMode == RendererVulkanMode::AUTO);

PacketSubmitResult SubmitPacket(IRenderer &renderer, std::unique_ptr<FramePacket> &packet)
{
    return renderer.TrySubmit(packet);
}

FrameCompileResult CompileGraph(DeviceFrameContext &context, const DeviceFrameLease &lease, std::shared_ptr<const FrozenFrameGraph> graph)
{
    return context.Compile(lease, std::move(graph));
}

int main()
{
    FramePacket          packet;
    PreparedFrame        prepared;
    FrameBuildInputs     inputs;
    RendererCreateResult create;
    FrameCompileResult   compile;
    FrameCollectResult   collect;
    return 0;
}

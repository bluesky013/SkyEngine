//
// Created by Zach Lee on 2022/8/1.
//

#include <render/RenderEncoder.h>
#include <render/DriverManager.h>
#include <render/DevObjManager.h>
#include <core/jobsystem/JobSystem.h>

namespace sky {

    bool RenderRasterEncoder::MultiThreadEncode() const
    {
        return producers.size() > MIN_DRAW_ITEM_PER_THREAD;
    }

    void RenderRasterEncoder::Encode(drv::GraphicsEncoder& encoder)
    {
        tf::Taskflow flow;
        uint32_t size = static_cast<uint32_t>(producers.size());
        if (MultiThreadEncode()) {
            uint32_t group = static_cast<uint32_t>(size / MIN_DRAW_ITEM_PER_THREAD + 1);
            drv::SecondaryCommands secondaryCommands;
            for (uint32_t i = 0; i < group; ++i) {
                flow.emplace([i, size, &encoder, &secondaryCommands, this]() {
                    auto queue = DriverManager::Get()->GetDevice()->GetQueue(VK_QUEUE_GRAPHICS_BIT);
                    auto &passInfo = encoder.GetCurrentPass();
                    uint32_t subPassId = encoder.GetSubPassId();

                    drv::CommandBuffer::Descriptor desc = {};
                    desc.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
                    desc.needFence = false;
                    auto command = queue->AllocateTlsCommandBuffer(desc);
                    VkCommandBufferInheritanceInfo inheritanceInfo{};
                    inheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
                    inheritanceInfo.renderPass = passInfo.renderPass;
                    inheritanceInfo.framebuffer = passInfo.framebuffer;
                    inheritanceInfo.subpass = subPassId;
                    command->Begin(inheritanceInfo);

                    auto tlsEncoder = command->EncodeGraphics();
                    tlsEncoder.SetViewport(1, &encoder.GetCurrentViewport());
                    tlsEncoder.SetScissor(1, &encoder.GetCurrentScissor());

                    for (uint32_t j = 0, k = i * MIN_DRAW_ITEM_PER_THREAD;
                         j < MIN_DRAW_ITEM_PER_THREAD && k < size; ++j, ++k) {
                        producers[k]->Process(tlsEncoder);
                    }
                    command->End();
                    secondaryCommands.Emplace(command);
                    DevObjManager::Get()->FreeDeviceObject(command);
                });
            }
            JobSystem::Get()->RunAndWait(flow);
            encoder.GetCommandBuffer().ExecuteSecondary(secondaryCommands);
        } else {
            for (auto &producer: producers) {
                producer->Process(encoder);
            }
        }
        producers.clear();
    }

    void RenderRasterEncoder::Emplace(const drv::DrawItem& item)
    {
        producers.emplace_back(new ItemDrawCallProducer(item)); // TODO : Pool
    }

    void RenderRasterEncoder::SetDrawTag(uint32_t tag)
    {
        drawTag = tag;
    }

    uint32_t RenderRasterEncoder::GetDrawTag() const
    {
        return drawTag;
    }

    void ItemDrawCallProducer::Process(drv::GraphicsEncoder& encoder)
    {
        encoder.Encode(item);
    }
}
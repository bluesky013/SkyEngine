//
// Created by Zach Lee on 2022/2/1.
//

#include <gtest/gtest.h>
#include <render/RenderBufferPool.h>
#include <render/DriverManager.h>

using namespace sky;

TEST(RenderTest, RenderBufferPoolTest)
{
    DriverManager::Get()->Initialize({});
    auto dev = DriverManager::Get()->GetDevice();
    auto& prop = dev->GetProperties();

    RenderBufferPool::Descriptor desc = {};
    desc.frame = 2;
    desc.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    desc.memory = VMA_MEMORY_USAGE_CPU_TO_GPU;
    desc.stride = (uint32_t)prop.limits.minUniformBufferOffsetAlignment;
    desc.blockSize = desc.stride * 4;


    {
        RenderBufferPool pool(desc);
        pool.Reserve(5);

        {
            auto bufferPair1 = pool.GetBuffer(0);
            auto bufferPair2 = pool.GetBuffer(1);
            auto bufferPair3 = pool.GetBuffer(2);
            auto bufferPair4 = pool.GetBuffer(3);
            auto bufferPair5 = pool.GetBuffer(4);

            ASSERT_EQ(bufferPair1.first.get(), bufferPair2.first.get());
            ASSERT_EQ(bufferPair1.second, 0);
            ASSERT_EQ(bufferPair2.second, desc.stride);

            ASSERT_NE(bufferPair1.first.get(), bufferPair3.first.get());

            ASSERT_EQ(bufferPair3.first.get(), bufferPair4.first.get());
            ASSERT_EQ(bufferPair3.second, 0);
            ASSERT_EQ(bufferPair4.second, desc.stride);

            ASSERT_NE(bufferPair3.first.get(), bufferPair5.first.get());
            ASSERT_EQ(bufferPair5.second, 0);
        }
        pool.SwapBuffer();
        {
            auto bufferPair1 = pool.GetBuffer(0);
            auto bufferPair2 = pool.GetBuffer(1);
            auto bufferPair3 = pool.GetBuffer(2);
            auto bufferPair4 = pool.GetBuffer(3);
            auto bufferPair5 = pool.GetBuffer(4);

            ASSERT_EQ(bufferPair1.first.get(), bufferPair2.first.get());
            ASSERT_EQ(bufferPair1.second, 0 + desc.blockSize / desc.frame);
            ASSERT_EQ(bufferPair2.second, desc.stride + desc.blockSize / desc.frame);

            ASSERT_NE(bufferPair1.first.get(), bufferPair3.first.get());

            ASSERT_EQ(bufferPair3.first.get(), bufferPair4.first.get());
            ASSERT_EQ(bufferPair3.second, 0 + desc.blockSize / desc.frame);
            ASSERT_EQ(bufferPair4.second, desc.stride + desc.blockSize / desc.frame);

            ASSERT_NE(bufferPair3.first.get(), bufferPair5.first.get());
            ASSERT_EQ(bufferPair5.second, 0 + desc.blockSize / desc.frame);
        }
        pool.SwapBuffer();
        {
            auto bufferPair1 = pool.GetBuffer(0);
            auto bufferPair2 = pool.GetBuffer(1);
            auto bufferPair3 = pool.GetBuffer(2);
            auto bufferPair4 = pool.GetBuffer(3);
            auto bufferPair5 = pool.GetBuffer(4);

            ASSERT_EQ(bufferPair1.first.get(), bufferPair2.first.get());
            ASSERT_EQ(bufferPair1.second, 0);
            ASSERT_EQ(bufferPair2.second, desc.stride);

            ASSERT_NE(bufferPair1.first.get(), bufferPair3.first.get());

            ASSERT_EQ(bufferPair3.first.get(), bufferPair4.first.get());
            ASSERT_EQ(bufferPair3.second, 0);
            ASSERT_EQ(bufferPair4.second, desc.stride);

            ASSERT_NE(bufferPair3.first.get(), bufferPair5.first.get());
            ASSERT_EQ(bufferPair5.second, 0);
        }
    }

    DriverManager::Get()->Destroy();
}
//
// Created by Zach Lee on 2022/5/27.
//

#include <gtest/gtest.h>
#include <engine/render/DriverManager.h>
#include <engine/render/resources/Mesh.h>
#include <engine/render/resources/Shader.h>

using namespace sky;

static const char* TAG = "EngineRenderResourceTest";

class EngineRenderResourceTest : public ::testing::Test {
public:
    static void SetUpTestSuite()
    {
        DriverManager::Get()->Initialize({"test"});
    }

    static void TearDownTestSuite()
    {
        DriverManager::Get()->Destroy();
    }

    void SetUp()
    {
    }

    void TearDown()
    {
    }
};

TEST_F(EngineRenderResourceTest, BufferTest)
{
    Buffer::Descriptor desc = {};
    desc.size = 128;
    desc.memory = VMA_MEMORY_USAGE_CPU_TO_GPU;
    desc.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    RDBufferPtr buffer = std::make_shared<Buffer>(desc);
    buffer->InitRHI();

    ASSERT_EQ(buffer->IsValid(), true);
}

TEST_F(EngineRenderResourceTest, ShaderTest)
{
    auto vs = std::make_shared<Shader>(Shader::Descriptor{VK_SHADER_STAGE_VERTEX_BIT});
    vs->LoadFromFile("shaders/BaseColor.vert.spv");
    vs->InitRHI();

    auto fs = std::make_shared<Shader>(Shader::Descriptor{VK_SHADER_STAGE_FRAGMENT_BIT});
    fs->LoadFromFile("shaders/BaseColor.frag.spv");
    fs->InitRHI();

    ASSERT_EQ(vs->IsValid(), true);
    ASSERT_EQ(fs->IsValid(), true);
}
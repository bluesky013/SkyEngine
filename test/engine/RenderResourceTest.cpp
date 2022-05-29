//
// Created by Zach Lee on 2022/5/27.
//

#include <gtest/gtest.h>
#include <engine/render/DriverManager.h>
#include <engine/render/resources/Mesh.h>
#include <engine/render/resources/Image.h>
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

struct Vertex {
    float pos[4];
    float normal[4];
    float color[4];
};

TEST_F(EngineRenderResourceTest, BufferTest)
{
    const uint32_t size = 128;
    std::vector<Vertex> vertices(size);

    {
        Buffer::Descriptor desc = {};
        desc.size = size * sizeof(Vertex);
        desc.memory = VMA_MEMORY_USAGE_CPU_TO_GPU;
        desc.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        RDBufferPtr buffer = std::make_shared<Buffer>(desc);
        buffer->InitRHI();
        buffer->Update(reinterpret_cast<const uint8_t *>(&vertices[0]), desc.size);
        ASSERT_EQ(buffer->IsValid(), true);
    }

    {
        Buffer::Descriptor desc = {};
        desc.size = size * sizeof(Vertex);
        desc.memory = VMA_MEMORY_USAGE_GPU_ONLY;
        desc.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        RDBufferPtr buffer = std::make_shared<Buffer>(desc);
        buffer->InitRHI();
        buffer->Update(reinterpret_cast<const uint8_t *>(&vertices[0]), desc.size);
        ASSERT_EQ(buffer->IsValid(), true);
    }
}

TEST_F(EngineRenderResourceTest, ImageTest)
{
    std::vector<uint8_t> data = {
        255, 0,   0, 255,   0, 255,   0, 255,
        0,   0, 255, 255, 255, 255, 255, 255
    };

    Image::Descriptor desc = {
        VK_FORMAT_R8G8B8A8_UNORM, {2, 2}, 1
    };
    RDImagePtr image = std::make_shared<Image>(desc);
    image->InitRHI();
    ASSERT_EQ(image->IsValid(), true);
    image->Update(data.data(), data.size());

    Texture::Descriptor texDes = {};
    auto tex = Texture::CreateFromImage(image, texDes);
    ASSERT_EQ(tex->IsValid(), true);
}

TEST_F(EngineRenderResourceTest, ImageLoadTest)
{
    auto image = Image::LoadFromFile("images/awesomeface.png");
}

TEST_F(EngineRenderResourceTest, ShaderTest)
{
    auto table = std::make_shared<GraphicsShaderTable>();
    table->LoadShader("shaders/BaseColor.vert.spv", "shaders/BaseColor.frag.spv");
    table->InitRHI();

    ASSERT_EQ(table->IsValid(), true);
}
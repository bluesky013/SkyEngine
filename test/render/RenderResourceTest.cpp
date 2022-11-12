//
// Created by Zach Lee on 2022/5/27.
//

#include <gtest/gtest.h>
#include <render/RHIManager.h>
#include <render/Render.h>
#include <render/resources/DescirptorGroup.h>
#include <render/resources/DescriptorPool.h>
#include <render/resources/Image.h>
#include <render/resources/Mesh.h>
#include <render/resources/Shader.h>

using namespace sky;

static const char *TAG = "EngineRenderResourceTest";

class EngineRenderResourceTest : public ::testing::Test {
public:
    static void SetUpTestSuite()
    {
        Render::Get()->Init({});
    }

    static void TearDownTestSuite()
    {
        Render::Get()->Destroy();
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
    const uint32_t      size = 128;
    std::vector<Vertex> vertices(size);

    {
        Buffer::Descriptor desc = {};
        desc.size               = size * sizeof(Vertex);
        desc.memory             = VMA_MEMORY_USAGE_CPU_TO_GPU;
        desc.usage              = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        RDBufferPtr buffer      = std::make_shared<Buffer>(desc);
        buffer->InitRHI();
        buffer->Update(reinterpret_cast<const uint8_t *>(&vertices[0]), desc.size);
        ASSERT_EQ(buffer->IsValid(), true);
    }

    {
        Buffer::Descriptor desc = {};
        desc.size               = size * sizeof(Vertex);
        desc.memory             = VMA_MEMORY_USAGE_GPU_ONLY;
        desc.usage              = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        RDBufferPtr buffer      = std::make_shared<Buffer>(desc);
        buffer->InitRHI();
        buffer->Update(reinterpret_cast<const uint8_t *>(&vertices[0]), desc.size);
        ASSERT_EQ(buffer->IsValid(), true);
    }
}

TEST_F(EngineRenderResourceTest, ImageTest)
{
    {
        std::vector<uint8_t> data = {255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 255, 255};

        Image::Descriptor desc  = {VK_FORMAT_R8G8B8A8_UNORM, {2, 2}, 1};
        RDImagePtr        image = std::make_shared<Image>(desc);
        image->InitRHI();
        ASSERT_EQ(image->IsValid(), true);
        image->Update(data.data(), data.size());

        Texture::Descriptor texDes = {};
        auto                tex    = Texture::CreateFromImage(image, texDes);
        ASSERT_EQ(tex->IsValid(), true);
    }

    {
        auto image = GlobalResource<Image>::Get()->GetResource(GlobalImageType::IMAGE_2D);
        ASSERT_EQ(image->IsValid(), true);

        Texture::Descriptor texDes = {};
        auto                tex    = Texture::CreateFromImage(image, texDes);
        ASSERT_EQ(tex->IsValid(), true);
    }

    GlobalResource<Image>::Get()->FreeAll();
}

TEST_F(EngineRenderResourceTest, DescriptorSetTest)
{
    vk::DescriptorSetLayout::VkDescriptor layoutDesc = {{
        {0, {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT}},
        {1, {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT}},
        {2, {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT}},
        {3, {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT}},
        {4, {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT}},
    }};
    auto                                 layout     = RHIManager::Get()->GetDevice()->CreateDeviceObject<vk::DescriptorSetLayout>(layoutDesc);
    ASSERT_EQ(!!layout, true);

    DescriptorPool::Descriptor desc = {2};
    auto                       pool = DescriptorPool::CreatePool(layout, desc);
    ASSERT_EQ(!!pool, true);

    {
        auto set1 = pool->Allocate();
        auto set2 = pool->Allocate();
        auto set3 = pool->Allocate();
        ASSERT_EQ(set1 && set1->GetRHISet(), true);
        ASSERT_EQ(set2 && set2->GetRHISet(), true);
        ASSERT_EQ(set3 && set3->GetRHISet(), true);
    }

    {
        auto set = pool->Allocate();
        ASSERT_EQ(!!set && set->GetRHISet(), true);
    }
}

// TEST_F(EngineRenderResourceTest, ImageLoadTest)
//{
//     auto image = Image::LoadFromFile("images/awesomeface.png");
// }
//
// TEST_F(EngineRenderResourceTest, ShaderTest)
//{
//     auto table = std::make_shared<GraphicsShaderTable>();
//     table->LoadShader("shaders/BaseColor.vert.spv", "shaders/BaseColor.frag.spv");
//     table->InitRHI();
//
//     ASSERT_EQ(table->IsValid(), true);
// }

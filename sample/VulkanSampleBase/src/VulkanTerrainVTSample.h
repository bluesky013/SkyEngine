//
// Created by Zach Lee on 2023/1/1.
//

#include "VulkanSampleBase.h"
#include <vulkan/GraphicsPipeline.h>
#include <vulkan/Shader.h>
#include <vulkan/SparseImage.h>
#include <vulkan/VertexInput.h>
#include <core/math/Vector4.h>

namespace sky {
    // first run "TerrainGenerator.exe -p D:\Code\Engine\SkyEngine\assets\ -b 16 -w 256"

    namespace sample {
        struct TerrainQuadData {
            uint32_t level;
            uint32_t padding[3];
        };

        struct TerrainLocalData {
            Vector4 data;
        };

        struct TerrainPage {
            vk::SparseImage::Page *page = nullptr;
            uint32_t level = ~(0U);
        };

        struct TerrainData {
            uint32_t                     width;
            uint32_t                     height;
            vk::SparseImagePtr           atlas;
            vk::SamplerPtr               sampler;
            vk::DescriptorSetPtr         set;
            vk::DescriptorSetPtr         localSet;
            vk::DescriptorSetBinderPtr   setBinder;
            TerrainLocalData             localData;
            vk::BufferPtr                localBuffer;
            vk::BufferPtr                quadBuffer;
            std::vector<TerrainQuadData> quadData;
            std::vector<TerrainPage>     pages;
            std::vector<std::string>     path;
        };

        struct PlayerLocalData {
            Vector4 data;
        };

        struct Player {
            PlayerLocalData            localData;
            vk::BufferPtr              localBuffer;
            vk::DescriptorSetPtr       set;
            vk::DescriptorSetBinderPtr setBinder;
        };
    }

    class VulkanTerrainVTSample : public VulkanSampleBase {
    public:
        VulkanTerrainVTSample()  = default;
        ~VulkanTerrainVTSample() = default;

        void OnTick(float delta) override;

        void OnStart() override;
        void OnStop() override;

    private:
        void SetupTerrain();
        void SetupPlayer();
        void SetupDescriptorSet();

        void PlayerUpdate(float delta);
        void UpdateBinding();
        void UpdateTerrainData();
        void InitFeature() override;

        void OnKeyUp(KeyButtonType button) override;
        void OnKeyDown(KeyButtonType button) override;

        vk::GraphicsPipelinePtr    pso;
        vk::PipelineLayoutPtr      pipelineLayout;
        vk::ShaderPtr              vs;
        vk::ShaderPtr              fs;
        vk::VertexInputPtr         vertexInput;

        vk::GraphicsPipelinePtr    playerPso;
        vk::PipelineLayoutPtr      playerLayout;
        vk::ShaderPtr              playerVs;
        vk::ShaderPtr              playerFs;

        vk::BufferPtr              globalBuffer;
        vk::DescriptorSetPtr       globalSet;
        vk::DescriptorSetPoolPtr   setPool;

        sample::TerrainData terrain;
        sample::Player player;

        std::unordered_map<KeyButtonType, bool> keys;
    };

} // namespace sky
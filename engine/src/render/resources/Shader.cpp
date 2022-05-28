//
// Created by Zach Lee on 2022/5/7.
//

#include <engine/render/resources/Shader.h>
#include <engine/render/DriverManager.h>
#include <core/file/FileIO.h>

namespace sky {

    Shader::Shader(const Descriptor& desc) : descriptor(desc)
    {
    }

    void Shader::LoadFromFile(const std::string& path)
    {
        ReadBin(path, spv);
    }

    void Shader::SetData(std::vector<uint32_t>&& data)
    {
        spv.swap(data);
    }

    void Shader::InitRHI()
    {
        drv::Shader::Descriptor desc = {};
        desc.size = static_cast<uint32_t>(spv.size()) * sizeof(uint32_t);
        desc.spv = spv.data();
        desc.stage = descriptor.stage;
        rhiShader = DriverManager::Get()->CreateDeviceObject<drv::Shader>(desc);
    }

    bool Shader::IsValid() const
    {
        return !!rhiShader;
    }

    void GraphicsShaderTable::LoadFromFile(const std::string &vsPath, const std::string &fsPath)
    {
        vs = std::make_shared<Shader>(Shader::Descriptor{VK_SHADER_STAGE_VERTEX_BIT});
        vs->LoadFromFile(vsPath);
        vs->InitRHI();

        fs = std::make_shared<Shader>(Shader::Descriptor{VK_SHADER_STAGE_FRAGMENT_BIT});
        fs->LoadFromFile(fsPath);
        fs->InitRHI();
    }

    bool GraphicsShaderTable::IsValid() const
    {
        return vs && fs;
    }

}
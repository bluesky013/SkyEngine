//
// Created by Zach Lee on 2022/8/13.
//

#include <RDSceneProject.h>
#include <framework/asset/AssetManager.h>
#include <RDSceneProject/EngineRoot.h>

namespace sky {

    static constexpr Uuid MODEL_ID = Uuid::CreateFromString("{8CF942CA-E5BF-407E-A9EC-9EC9BBAB70CA}");

    void RDSceneProject::Init()
    {
        AssetManager::Get()->RegisterSearchPath(ENGINE_ROOT);
        AssetManager::Get()->RegisterSearchPath(PROJECT_ROOT);
        AssetManager::Get()->RegisterAsset(MODEL_ID, "data/models/medi2_buffer.bin");
    }

    void RDSceneProject::Start()
    {
        auto bufferAsset = AssetManager::Get()->LoadAsset<Buffer>(MODEL_ID);

    }

    void RDSceneProject::Stop()
    {

    }

    void RDSceneProject::Tick(float delta)
    {

    }

}

REGISTER_MODULE(sky::RDSceneProject)
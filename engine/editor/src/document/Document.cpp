//
// Created by Zach Lee on 2023/1/14.
//

#include <editor/document/Document.h>
#include <QFile>
#include <framework/serialization/SerializationContext.h>
#include <framework/asset/AssetDataBase.h>
#include <physics/PhysicsRegistry.h>
#include <physics/PhysicsWorld.h>
#include <navigation/NavigationSystem.h>
#include <render/adaptor/RenderSceneProxy.h>

namespace sky::editor {

    Document::Document()
    {
        SetFlag(DocumentFlagBit::ProjectOpen);
        AssetDataBase::Get()->Load();
    }

    Document::~Document()
    {
        ResetFlag(DocumentFlagBit::ProjectOpen);
        AssetDataBase::Get()->Save();
    }

    void Document::SetFlag(DocumentFlagBit bit)
    {
        flags.SetBit(bit);
    }

    void Document::ResetFlag(DocumentFlagBit bit)
    {
        flags.ResetBit(bit);
    }

    const DocFlagArray &Document::GetFlag() const
    {
        return flags;
    }

    void Document::Reflect(SerializationContext *context)
    {
        context->Register<ProjectData>("ProjectData")
            .Member<&ProjectData::version>("version");
    }

    void Document::LoadWorld()
    {
        auto absPath = AssetDataBase::Get()->GetWorkSpaceFs()->GetPath() / FilePath(filePath.toStdString());
        NativeFile file(absPath);
        auto archive = file.ReadAsArchive();
        if (!archive || !archive->IsOpen()) {
            return;
        }

        JsonInputArchive json(*archive);
        world->LoadJson(json);
    }

    void Document::SaveWorld()
    {
        auto absPath = AssetDataBase::Get()->GetWorkSpaceFs()->GetPath() / FilePath(filePath.toStdString());
        NativeFile file(absPath);
        auto archive = file.WriteAsArchive();
        JsonOutputArchive json(*archive);
        world->SaveJson(json);
    }

    bool Document::NeedSave() const
    {
        return world != nullptr;
    }

    WorldPtr Document::OpenWorld(const QString &path)
    {
        CloseWorld();

        world = World::CreateWorld();
        world->Init();

        auto workPath = AssetDataBase::Get()->GetWorkSpaceFs()->GetPath();
        auto fullPath = FilePath(path.toStdString());
        auto relativePath = fullPath.Relative(workPath);

        AssetSourcePath worldPath = {
            .bundle = SourceAssetBundle::WORKSPACE,
            .path = relativePath
        };
        auto worldUuid = AssetDataBase::CalculateUuidByPath(worldPath);
        world->SetPersistID(worldUuid);
        world->AddSubSystem(Name(RenderSceneProxy::NAME.data()), new RenderSceneProxy(worldUuid));

        auto *physics = phy::PhysicsRegistry::Get()->CreatePhysicsWorld();
        if (physics != nullptr) {
            world->AddSubSystem(Name(phy::PhysicsWorld::NAME.data()), physics);
        }

        world->AddSubSystem(Name(ai::NavigationSystem::NAME.data()), new ai::NavigationSystem());

        filePath = path;
        LoadWorld();
        SetFlag(DocumentFlagBit::WorldOpen);
        return world;
    }

    void Document::CloseWorld()
    {
        if (!world) {
            return;
        }
        SaveWorld();
        world = nullptr;
        ResetFlag(DocumentFlagBit::WorldOpen);
    }

} // namespace sky::editor

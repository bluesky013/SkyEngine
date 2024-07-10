//
// Created by Zach Lee on 2023/1/14.
//

#include <editor/document/Document.h>
#include <QFileInfo>
#include <QFile>
#include <QTextStream>
#include <framework/serialization/SerializationContext.h>
#include <framework/asset/AssetManager.h>

namespace sky::editor {

    Document::Document()
    {
        SetFlag(DocumentFlagBit::ProjectOpen);
    }

    Document::~Document()
    {
        ResetFlag(DocumentFlagBit::ProjectOpen);
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

    void Document::Reflect()
    {
        SerializationContext::Get()->Register<ProjectData>("ProjectData")
            .Member<&ProjectData::version>("version");
    }

    void Document::LoadWorld()
    {
        NativeFile file(FilePath(filePath.toStdString()));
        auto archive = file.ReadAsArchive();
        if (!archive || !archive->IsOpen()) {
            return;
        }

        JsonInputArchive json(*archive);
        world->LoadJson(json);
    }

    void Document::SaveWorld()
    {
        NativeFile file(FilePath(filePath.toStdString()));
        auto archive = file.WriteAsArchive();
        JsonOutputArchive json(*archive);

        world->SaveJson(json);
    }

    WorldPtr Document::OpenWorld(const QString &path)
    {
        CloseWorld();

        world = World::CreateWorld();
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

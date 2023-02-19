//
// Created by Zach Lee on 2023/1/14.
//

#include <editor/document/Document.h>
#include <QFileInfo>
#include <QDir>
#include <fstream>
#include <framework/serialization/JsonArchive.h>
#include <framework/serialization/SerializationContext.h>
#include <framework/asset/AssetManager.h>

namespace sky::editor {

    Document::Document(const QString &path)
    {
        projectFullPath = path;
    }

    Document::~Document()
    {
        ResetFlag(DocumentFlagBit::PROJECT_OPEN);
        ResetFlag(DocumentFlagBit::LEVEL_OPEN);
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

    const WorldPtr &Document::GetMainWorld() const
    {
        return currentLevel->GetWorld();
    }

    void Document::Reflect()
    {
        SerializationContext::Get()->Register<ProjectData>("ProjectData")
            .Member<&ProjectData::version>("version");
    }

    void Document::Init()
    {
        QFileInfo file(projectFullPath);
        if (!file.exists()) {
            Save();
        } else {
            Read();
        }

        projectHome = file.path();
        auto mkdir = [&](const QString &path) {
            QDir dir(projectHome);
            QDir tmp(projectHome + QDir::separator() + path);
            if (!tmp.exists()) {
                dir.mkdir(path);
            }
        };
        mkdir("assets");
        mkdir("levels");

        SetFlag(DocumentFlagBit::PROJECT_OPEN);

        AssetManager::Get()->Reset(projectHome.toStdString() + "/assets.db");
    }

    void Document::Read()
    {
        auto str = projectFullPath.toStdString();
        std::ifstream file(str, std::ios::binary);
        JsonInputArchive archive(file);
        archive.LoadValueObject(projectData);
    }

    void Document::Save()
    {
        auto str = projectFullPath.toStdString();
        std::ofstream file(str, std::ios::binary);
        JsonOutputArchive archive(file);
        archive.SaveValueObject(projectData);
    }

    void Document::OpenLevel(const QString &path, bool newLevel)
    {
        currentLevel.reset(new Level());
        if (newLevel) {
            currentLevel->New(projectHome + QDir::separator() + "levels" + QDir::separator() + path);
        } else {
            currentLevel->Open(path);
        }
        SetFlag(DocumentFlagBit::LEVEL_OPEN);
    }

    void Document::CloseLevel()
    {
        currentLevel.reset();
        ResetFlag(DocumentFlagBit::LEVEL_OPEN);
    }

} // namespace sky::editor

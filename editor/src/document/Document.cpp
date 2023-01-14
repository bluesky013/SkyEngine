//
// Created by Zach Lee on 2023/1/14.
//

#include <editor/document/Document.h>
#include <QFileInfo>
#include <QDir>
#include <fstream>
#include <cereal/cereal.hpp>
#include <cereal/archives/json.hpp>

namespace sky::editor {

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

    void Document::Init()
    {
        QFileInfo file(projectFullPath);
        if (!file.exists()) {
            Save();
        } else {
            Read();
        }

        auto mkdir = [&](const QString &path) {
            QDir dir(file.path());
            QDir tmp(file.path() + QDir::separator() + path);
            if (!tmp.exists()) {
                dir.mkdir(path);
            }
        };
        mkdir("assets");
        mkdir("levels");

        SetFlag(DocumentFlagBit::PROJECT_OPEN);
    }

    void Document::Read()
    {
        auto str = projectFullPath.toStdString();
        std::ifstream file(str, std::ios::binary);
        cereal::JSONInputArchive archive(file);
        archive >> projectData;
    }

    void Document::Save()
    {
        auto str = projectFullPath.toStdString();
        std::ofstream file(str, std::ios::binary);
        cereal::JSONOutputArchive archive(file);
        archive << projectData;
    }

} // namespace sky::editor
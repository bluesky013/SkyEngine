//
// Created by Zach Lee on 2023/1/14.
//

#pragma once

#include <vector>
#include <QString>
#include <editor/document/Constants.h>
#include <core/file/FileSystem.h>
#include <framework/world/World.h>

namespace sky::editor {

    struct ProjectData {
        uint32_t version = 0;
    };

    class Document {
    public:
        explicit Document();
        ~Document();

        static void Reflect();

        WorldPtr OpenWorld(const QString &path);
        void CloseWorld();

        void LoadWorld();
        void SaveWorld();

        void SetFlag(DocumentFlagBit bit);
        void ResetFlag(DocumentFlagBit bit);
        const DocFlagArray &GetFlag() const;

    private:
        DocFlagArray flags;
        ProjectData projectData;

        QString filePath;
        WorldPtr world;
    };

} // namespace sky::editor

//
// Created by Zach Lee on 2023/1/14.
//

#pragma once

#include <vector>
#include <QString>
#include <editor/document/Constants.h>
#include <editor/document/Level.h>

namespace sky::editor {

    struct ProjectData {
        uint32_t version = 0;
    };

    class Document {
    public:
        Document(const QString &path);
        ~Document() = default;

        static void Reflect();

        void Init();
        void Read();
        void Save();

        void OpenLevel(const QString &path, bool newLevel);
        void CloseLevel();

        void SetFlag(DocumentFlagBit bit);
        void ResetFlag(DocumentFlagBit bit);
        const DocFlagArray &GetFlag() const;

        const WorldPtr &GetMainWorld() const;

    private:
        DocFlagArray flags;
        QString projectFullPath;
        QString projectHome;

        ProjectData projectData;

        std::unique_ptr<Level> currentLevel;
    };

} // namespace sky::editor

//
// Created by Zach Lee on 2023/1/14.
//

#pragma once

#include <vector>
#include <QString>
#include <editor/document/Constants.h>

namespace sky::editor {

    struct ProjectData {
        uint32_t version = 0;

        template <typename Archive>
        void serialize(Archive &ar)
        {
            ar(version);
        }
    };

    class Document {
    public:
        Document(const QString &path) : projectFullPath(path) {}
        ~Document() = default;

        void Init();
        void Read();
        void Save();

        void SetFlag(DocumentFlagBit bit);
        void ResetFlag(DocumentFlagBit bit);
        const DocFlagArray &GetFlag() const;

    private:
        DocFlagArray flags;
        QString projectFullPath;
        ProjectData projectData;
    };

} // namespace sky::editor
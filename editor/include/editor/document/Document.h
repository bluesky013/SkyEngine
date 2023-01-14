//
// Created by Zach Lee on 2023/1/14.
//

#pragma once

#include <vector>
#include <QString>
#include <editor/document/Constants.h>

namespace sky::editor {

    class Document {
    public:
        Document(const QString &path) : projectPath(path) {}
        ~Document() = default;

        void SetFlag(DocumentFlagBit bit);
        void ResetFlag(DocumentFlagBit bit);
        const DocFlagArray &GetFlag() const;

    private:
        DocFlagArray flags;
        QString projectPath;
    };

} // namespace sky::editor
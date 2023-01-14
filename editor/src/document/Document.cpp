//
// Created by Zach Lee on 2023/1/14.
//

#include <editor/document/Document.h>

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

} // namespace sky::editor
//
// Created by blues on 2024/10/5.
//

#include <framework/serialization/ArrayVisitor.h>

namespace sky {

    void* SequenceVisitor::Emplace()
    {
        return info->sequenceView->Emplace(object);
    }

    void SequenceVisitor::Erase(size_t index)
    {
        info->sequenceView->EraseByIndex(object, index);
    }

    void* SequenceVisitor::GetByIndex(size_t index)
    {
        return info->sequenceView->GetByIndex(object, index);
    }

    size_t SequenceVisitor::Count() const
    {
        return info->sequenceView->Count(object);
    }

    const Uuid &SequenceVisitor::GetValueType() const
    {
        return info->sequenceView != nullptr ? info->valueType : Uuid::GetEmpty();
    }

} // namespace sky
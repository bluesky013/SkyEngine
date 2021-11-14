//
// Created by Zach Lee on 2021/11/13.
//


#include <world/TransformComponent.h>

namespace sky {

    void TransformComponent::SetParent(TransformComponent* newParent)
    {
        auto findChild = [](std::vector<TransformComponent*>& trans, TransformComponent* current) {
            return std::find(trans.begin(), trans.end(), current);
        };
        if (parent != nullptr) {
            auto iter = findChild(parent->children, this);
            if (iter != parent->children.end()) {
                parent->children.erase(iter);
            }
        }
        parent = newParent;

        auto iter = findChild(parent->children, this);
        if (iter == parent->children.end()) {
            newParent->children.emplace_back();
        }
    }

}
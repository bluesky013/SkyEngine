//
// Created by Zach Lee on 2021/11/13.
//

#include <engine/world/TransformComponent.h>
#include <engine/world/GameObject.h>
#include <core/logger/Logger.h>
#include <string>

namespace sky {

    static const char* TAG = "TransformComponent";

    static auto FindChild(std::vector<TransformComponent*>& trans, TransformComponent* current)
    {
        return std::find(trans.begin(), trans.end(), current);
    }

    TransformComponent::~TransformComponent()
    {
        if (parent != nullptr) {
            auto iter = FindChild(parent->children, this);
            if (iter != parent->children.end()) {
                parent->children.erase(iter);
            }
        }
        for (auto& child : children) {
            delete child->object;
        }
        children.clear();
    }

    void TransformComponent::SetParent(TransformComponent* newParent)
    {
        if (parent != nullptr) {
            auto iter = FindChild(parent->children, this);
            if (iter != parent->children.end()) {
                parent->children.erase(iter);
            }
        }
        parent = newParent;

        auto iter = FindChild(parent->children, this);
        if (iter == parent->children.end()) {
            newParent->children.emplace_back(this);
        }
    }

    TransformComponent* TransformComponent::GetParent() const
    {
        return parent;
    }

    const std::vector<TransformComponent*>& TransformComponent::GetChildren() const
    {
        return children;
    }

    void TransformComponent::Update()
    {
    }

    void TransformComponent::PrintChild(TransformComponent& comp, std::string str)
    {
        LOG_I(TAG, "%s%s", str.c_str(), comp.object->GetName().c_str());
        for (auto& child : comp.children) {
            PrintChild(*child, str + "  ");
        }
    }

    void TransformComponent::Print()
    {
        PrintChild(*this, "");
    }
}
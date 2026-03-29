//
// Created by blues on 2025/4/21.
//

#include <shader/node/ResourceGroupDecl.h>
#include <unordered_set>

namespace sky::sl {

    const StructDecl *FindStructInGroup(std::string_view name, const ResourceGroupDecl &group)
    {
        for (const auto &s : group.localStructDecls) {
            if (s.name == name) {
                return &s;
            }
        }
        return nullptr;
    }

    static void CollectStructRefsFromMembers(std::span<const MemberDecl> members,
                                             const ResourceGroupDecl &group,
                                             std::unordered_set<std::string_view> &visited,
                                             std::vector<const StructDecl*> &result)
    {
        for (const auto &m : members) {
            if (m.type.dataType == ShaderDataType::STRUCT && !m.structRef.empty()) {
                if (visited.count(m.structRef)) {
                    continue;
                }
                visited.insert(m.structRef);

                const StructDecl *found = FindStructInGroup(m.structRef, group);
                if (found) {
                    // Recurse into struct members (dependency-first)
                    CollectStructRefsFromMembers(found->members, group, visited, result);
                    result.push_back(found);
                }
            }
        }
    }

    std::vector<const StructDecl*> CollectReferencedStructs(const ResourceGroupDecl &group)
    {
        std::unordered_set<std::string_view> visited;
        std::vector<const StructDecl*> result;

        auto processResources = [&](std::span<const ResourceDecl> resources) {
            for (const auto &res : resources) {
                if (res.type == ResourceType::CONSTANT_BUFFER) {
                    CollectStructRefsFromMembers(res.members, group, visited, result);
                }
                if (res.type == ResourceType::STRUCTURED_BUFFER) {
                    if (!res.elementStructRef.empty() && !visited.count(res.elementStructRef)) {
                        visited.insert(res.elementStructRef);
                        const StructDecl *found = FindStructInGroup(res.elementStructRef, group);
                        if (found) {
                            CollectStructRefsFromMembers(found->members, group, visited, result);
                            result.push_back(found);
                        }
                    }
                }
            }
        };

        processResources(group.resources);
        for (const auto &cond : group.conditionals) {
            processResources(cond.resources);
        }

        return result;
    }

} // namespace sky::sl

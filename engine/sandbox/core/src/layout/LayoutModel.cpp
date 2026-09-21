//
// Created on 2026/09/21.
//

#include <editor/core/layout/LayoutModel.h>
#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <algorithm>
#include <cmath>

namespace sky::editor {

    namespace {

        constexpr float kMinRatio = 0.05f;
        constexpr float kMaxRatio = 0.95f;

        LayoutNodePtr MakeTab(const std::string &panelId)
        {
            auto tab = std::make_unique<TabNode>();
            tab->panels.push_back(PanelNode{panelId});
            tab->activeIndex = 0;
            return tab;
        }

        void NormalizeRatios(SplitNode &split)
        {
            if (split.children.empty()) {
                return;
            }
            if (split.ratios.size() != split.children.size()) {
                split.ratios.assign(split.children.size(), 1.0f / static_cast<float>(split.children.size()));
            }
            float sum = 0.0f;
            for (float ratio : split.ratios) {
                sum += ratio;
            }
            if (sum <= 0.0f) {
                split.ratios.assign(split.children.size(), 1.0f / static_cast<float>(split.children.size()));
                return;
            }
            for (float &ratio : split.ratios) {
                ratio /= sum;
            }
        }

        void Collapse(LayoutNodePtr &slot)
        {
            if (slot == nullptr) {
                return;
            }
            if (slot->kind == LayoutNodeKind::TAB) {
                auto *tab = static_cast<TabNode *>(slot.get());
                if (tab->panels.empty()) {
                    slot.reset();
                    return;
                }
                if (tab->activeIndex < 0 || tab->activeIndex >= static_cast<int32_t>(tab->panels.size())) {
                    tab->activeIndex = static_cast<int32_t>(tab->panels.size()) - 1;
                }
                return;
            }

            auto *split = static_cast<SplitNode *>(slot.get());
            for (auto &child : split->children) {
                Collapse(child);
            }
            split->children.erase(std::remove_if(split->children.begin(), split->children.end(),
                                                 [](const LayoutNodePtr &child) { return child == nullptr; }),
                                  split->children.end());
            if (split->children.empty()) {
                slot.reset();
                return;
            }
            if (split->children.size() == 1) {
                slot = std::move(split->children[0]);
                Collapse(slot);
                return;
            }
            NormalizeRatios(*split);
        }

        LayoutNode *FindParentImpl(LayoutNode *node, const LayoutNode *target)
        {
            if (node == nullptr || node->kind != LayoutNodeKind::SPLIT) {
                return nullptr;
            }
            auto *split = static_cast<SplitNode *>(node);
            for (auto &child : split->children) {
                if (child.get() == target) {
                    return split;
                }
                if (auto *found = FindParentImpl(child.get(), target)) {
                    return found;
                }
            }
            return nullptr;
        }

        TabNode *FindTabImpl(LayoutNode *node, const std::string &panelId)
        {
            if (node == nullptr) {
                return nullptr;
            }
            if (node->kind == LayoutNodeKind::TAB) {
                auto *tab = static_cast<TabNode *>(node);
                for (const auto &panel : tab->panels) {
                    if (panel.panelId == panelId) {
                        return tab;
                    }
                }
                return nullptr;
            }
            for (auto &child : static_cast<SplitNode *>(node)->children) {
                if (auto *found = FindTabImpl(child.get(), panelId)) {
                    return found;
                }
            }
            return nullptr;
        }

        void CollectImpl(const LayoutNode *node, std::vector<std::string> &out)
        {
            if (node == nullptr) {
                return;
            }
            if (node->kind == LayoutNodeKind::TAB) {
                for (const auto &panel : static_cast<const TabNode *>(node)->panels) {
                    out.push_back(panel.panelId);
                }
                return;
            }
            for (const auto &child : static_cast<const SplitNode *>(node)->children) {
                CollectImpl(child.get(), out);
            }
        }

        const char *OrientationToText(SplitOrientation orientation)
        {
            return orientation == SplitOrientation::VERTICAL ? "v" : "h";
        }

        rapidjson::Value NodeToJson(const LayoutNode *node, rapidjson::Document::AllocatorType &allocator)
        {
            rapidjson::Value value(rapidjson::kObjectType);
            if (node->kind == LayoutNodeKind::TAB) {
                const auto *tab = static_cast<const TabNode *>(node);
                value.AddMember("type", "tab", allocator);
                rapidjson::Value panels(rapidjson::kArrayType);
                for (const auto &panel : tab->panels) {
                    rapidjson::Value id;
                    id.SetString(panel.panelId.c_str(), static_cast<rapidjson::SizeType>(panel.panelId.size()),
                                 allocator);
                    panels.PushBack(id, allocator);
                }
                value.AddMember("panels", panels, allocator);
                value.AddMember("active", tab->activeIndex, allocator);
            } else {
                const auto *split = static_cast<const SplitNode *>(node);
                value.AddMember("type", "split", allocator);
                value.AddMember("orientation", rapidjson::StringRef(OrientationToText(split->orientation)), allocator);
                rapidjson::Value ratios(rapidjson::kArrayType);
                for (float ratio : split->ratios) {
                    ratios.PushBack(ratio, allocator);
                }
                value.AddMember("ratios", ratios, allocator);
                rapidjson::Value children(rapidjson::kArrayType);
                for (const auto &child : split->children) {
                    children.PushBack(NodeToJson(child.get(), allocator), allocator);
                }
                value.AddMember("children", children, allocator);
            }
            return value;
        }

        LayoutNodePtr NodeFromJson(const rapidjson::Value &value, const PanelRegistry *registry,
                                   std::vector<std::string> *warnings)
        {
            if (!value.IsObject()) {
                return nullptr;
            }
            const auto typeIt = value.FindMember("type");
            if (typeIt == value.MemberEnd() || !typeIt->value.IsString()) {
                return nullptr;
            }
            const std::string type = typeIt->value.GetString();

            if (type == "tab") {
                auto tab = std::make_unique<TabNode>();
                const auto panelsIt = value.FindMember("panels");
                if (panelsIt != value.MemberEnd() && panelsIt->value.IsArray()) {
                    for (const auto &panel : panelsIt->value.GetArray()) {
                        if (!panel.IsString()) {
                            continue;
                        }
                        const std::string id = panel.GetString();
                        if (registry != nullptr && !registry->Contains(id)) {
                            if (warnings != nullptr) {
                                warnings->push_back("unknown panel: " + id);
                            }
                            continue;
                        }
                        tab->panels.push_back(PanelNode{id});
                    }
                }
                if (tab->panels.empty()) {
                    return nullptr;
                }
                const auto activeIt = value.FindMember("active");
                tab->activeIndex =
                    activeIt != value.MemberEnd() && activeIt->value.IsInt() ? activeIt->value.GetInt() : 0;
                if (tab->activeIndex < 0 || tab->activeIndex >= static_cast<int32_t>(tab->panels.size())) {
                    tab->activeIndex = 0;
                }
                return tab;
            }

            if (type == "split") {
                auto split = std::make_unique<SplitNode>();
                const auto orientationIt = value.FindMember("orientation");
                if (orientationIt != value.MemberEnd() && orientationIt->value.IsString() &&
                    std::string(orientationIt->value.GetString()) == "v") {
                    split->orientation = SplitOrientation::VERTICAL;
                }
                const auto childrenIt = value.FindMember("children");
                if (childrenIt != value.MemberEnd() && childrenIt->value.IsArray()) {
                    for (const auto &child : childrenIt->value.GetArray()) {
                        if (auto node = NodeFromJson(child, registry, warnings)) {
                            split->children.push_back(std::move(node));
                        }
                    }
                }
                if (split->children.empty()) {
                    return nullptr;
                }
                const auto ratiosIt = value.FindMember("ratios");
                if (ratiosIt != value.MemberEnd() && ratiosIt->value.IsArray()) {
                    for (const auto &ratio : ratiosIt->value.GetArray()) {
                        if (ratio.IsNumber()) {
                            split->ratios.push_back(ratio.GetFloat());
                        }
                    }
                }
                if (split->children.size() == 1) {
                    return std::move(split->children[0]);
                }
                NormalizeRatios(*split);
                return split;
            }
            return nullptr;
        }

    } // namespace

    void LayoutModel::SetDefault(std::vector<std::string> panelIds)
    {
        defaultPanels = panelIds;
        hasDefault = true;

        root = std::make_unique<TabNode>();
        auto *tab = static_cast<TabNode *>(root.get());
        for (auto &id : panelIds) {
            tab->panels.push_back(PanelNode{std::move(id)});
        }
        tab->activeIndex = 0;
    }

    void LayoutModel::ResetToDefault()
    {
        if (!hasDefault) {
            Clear();
            return;
        }
        SetDefault(defaultPanels);
    }

    void LayoutModel::Clear()
    {
        root.reset();
    }

    bool LayoutModel::SplitPanel(const std::string &panelId, SplitOrientation orientation, const std::string &newPanelId)
    {
        TabNode *tab = FindTab(panelId);
        if (tab == nullptr) {
            return false;
        }

        auto split = std::make_unique<SplitNode>();
        split->orientation = orientation;
        split->ratios = {0.5f, 0.5f};

        LayoutNode *parent = FindParent(tab);
        if (parent == nullptr) {
            split->children.push_back(std::move(root));
            split->children.push_back(MakeTab(newPanelId));
            root = std::move(split);
            return true;
        }

        auto *parentSplit = static_cast<SplitNode *>(parent);
        for (auto &child : parentSplit->children) {
            if (child.get() == tab) {
                split->children.push_back(std::move(child));
                split->children.push_back(MakeTab(newPanelId));
                child = std::move(split);
                NormalizeRatios(*parentSplit);
                return true;
            }
        }
        return false;
    }

    bool LayoutModel::MoveToArea(const std::string &panelId, const std::string &targetPanelId)
    {
        return Tabify(panelId, targetPanelId);
    }

    bool LayoutModel::Tabify(const std::string &panelId, const std::string &targetPanelId)
    {
        if (panelId == targetPanelId) {
            return false;
        }
        TabNode *source = FindTab(panelId);
        TabNode *target = FindTab(targetPanelId);
        if (source == nullptr || target == nullptr) {
            return false;
        }

        const auto sourceIt = std::find_if(source->panels.begin(), source->panels.end(),
                                           [&](const PanelNode &panel) { return panel.panelId == panelId; });
        if (sourceIt == source->panels.end()) {
            return false;
        }
        source->panels.erase(sourceIt);

        const auto targetIt = std::find_if(target->panels.begin(), target->panels.end(),
                                           [&](const PanelNode &panel) { return panel.panelId == targetPanelId; });
        const size_t position = targetIt == target->panels.end()
            ? target->panels.size()
            : static_cast<size_t>(targetIt - target->panels.begin()) + 1;
        target->panels.insert(target->panels.begin() + static_cast<std::ptrdiff_t>(position), PanelNode{panelId});
        target->activeIndex = static_cast<int32_t>(position);

        Collapse(root);
        return true;
    }

    bool LayoutModel::ClosePanel(const std::string &panelId)
    {
        TabNode *tab = FindTab(panelId);
        if (tab == nullptr) {
            return false;
        }
        const auto it = std::find_if(tab->panels.begin(), tab->panels.end(),
                                     [&](const PanelNode &panel) { return panel.panelId == panelId; });
        if (it == tab->panels.end()) {
            return false;
        }
        tab->panels.erase(it);
        Collapse(root);
        return true;
    }

    bool LayoutModel::SetRatio(LayoutNode *splitNode, uint32_t index, float ratio)
    {
        if (splitNode == nullptr || splitNode->kind != LayoutNodeKind::SPLIT) {
            return false;
        }
        auto *split = static_cast<SplitNode *>(splitNode);
        if (split->ratios.size() < 2 || index + 1 >= split->ratios.size()) {
            return false;
        }

        ratio = std::clamp(ratio, kMinRatio, kMaxRatio);

        float others = 0.0f;
        for (uint32_t i = 0; i < split->ratios.size(); ++i) {
            if (i != index) {
                others += split->ratios[i];
            }
        }

        const float remaining = 1.0f - ratio;
        if (others <= 0.0f) {
            const float even = remaining / static_cast<float>(split->ratios.size() - 1);
            for (uint32_t i = 0; i < split->ratios.size(); ++i) {
                split->ratios[i] = i == index ? ratio : even;
            }
        } else {
            const float scale = remaining / others;
            for (uint32_t i = 0; i < split->ratios.size(); ++i) {
                if (i != index) {
                    split->ratios[i] *= scale;
                }
            }
            split->ratios[index] = ratio;
        }
        return true;
    }

    void LayoutModel::CollectPanels(std::vector<std::string> &out) const
    {
        CollectImpl(root.get(), out);
    }

    TabNode *LayoutModel::FindTab(const std::string &panelId) const
    {
        return FindTabImpl(root.get(), panelId);
    }

    LayoutNode *LayoutModel::FindParent(const LayoutNode *node) const
    {
        return FindParentImpl(root.get(), node);
    }

    std::string LayoutModel::ToJson(int indent) const
    {
        rapidjson::Document document;
        document.SetObject();
        auto &allocator = document.GetAllocator();
        document.AddMember("version", version, allocator);
        if (root != nullptr) {
            document.AddMember("root", NodeToJson(root.get(), allocator), allocator);
        }

        rapidjson::StringBuffer buffer;
        if (indent >= 0) {
            rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
            writer.SetIndent(' ', static_cast<unsigned>(indent));
            document.Accept(writer);
        } else {
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            document.Accept(writer);
        }
        return buffer.GetString();
    }

    bool LayoutModel::FromJson(const std::string &json, LayoutModel &out, const PanelRegistry *registry,
                               std::vector<std::string> *warnings)
    {
        rapidjson::Document document;
        document.Parse(json.c_str(), json.size());
        if (document.HasParseError() || !document.IsObject()) {
            return false;
        }

        out.root.reset();
        const auto versionIt = document.FindMember("version");
        out.version = versionIt != document.MemberEnd() && versionIt->value.IsInt() ? versionIt->value.GetInt() : 1;

        const auto rootIt = document.FindMember("root");
        if (rootIt != document.MemberEnd()) {
            out.root = NodeFromJson(rootIt->value, registry, warnings);
        }
        Collapse(out.root);
        return true;
    }

} // namespace sky::editor

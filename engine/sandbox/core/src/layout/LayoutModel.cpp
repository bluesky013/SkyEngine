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

        void RemovePanelFromTab(TabNode *tab, const std::string &panelId)
        {
            if (tab == nullptr) {
                return;
            }
            const auto it = std::find_if(tab->panels.begin(), tab->panels.end(),
                                         [&](const PanelNode &panel) { return panel.panelId == panelId; });
            if (it == tab->panels.end()) {
                return;
            }
            const int32_t index = static_cast<int32_t>(it - tab->panels.begin());
            tab->panels.erase(it);
            if (tab->activeIndex > index) {
                --tab->activeIndex;
            }
            if (tab->activeIndex >= static_cast<int32_t>(tab->panels.size())) {
                tab->activeIndex = static_cast<int32_t>(tab->panels.size()) - 1;
            }
        }

        // Places newTab next to target inside the tree rooted at rootSlot. When
        // the target's parent already splits along the required axis the new tab
        // is inserted there; otherwise the target is wrapped in a new split.
        bool InsertBeside(LayoutNodePtr &rootSlot, LayoutNode *target, LayoutNodePtr newTab, DockPosition position)
        {
            const SplitOrientation want =
                (position == DockPosition::LEFT || position == DockPosition::RIGHT) ? SplitOrientation::HORIZONTAL
                                                                                   : SplitOrientation::VERTICAL;
            const bool before = (position == DockPosition::LEFT || position == DockPosition::TOP);

            LayoutNode *parent = FindParentImpl(rootSlot.get(), target);
            if (parent == nullptr) {
                if (rootSlot.get() != target) {
                    return false;
                }
                auto split = std::make_unique<SplitNode>();
                split->orientation = want;
                split->ratios = {0.5f, 0.5f};
                if (before) {
                    split->children.push_back(std::move(newTab));
                    split->children.push_back(std::move(rootSlot));
                } else {
                    split->children.push_back(std::move(rootSlot));
                    split->children.push_back(std::move(newTab));
                }
                rootSlot = std::move(split);
                return true;
            }

            auto *parentSplit = static_cast<SplitNode *>(parent);
            if (parentSplit->orientation == want) {
                size_t index = 0;
                for (size_t i = 0; i < parentSplit->children.size(); ++i) {
                    if (parentSplit->children[i].get() == target) {
                        index = i;
                        break;
                    }
                }
                const float base = index < parentSplit->ratios.size()
                    ? parentSplit->ratios[index]
                    : (1.0f / static_cast<float>(parentSplit->children.size()));
                if (index < parentSplit->ratios.size()) {
                    parentSplit->ratios[index] = base * 0.5f;
                }
                const size_t insertAt = before ? index : index + 1;
                parentSplit->children.insert(parentSplit->children.begin() + static_cast<std::ptrdiff_t>(insertAt),
                                             std::move(newTab));
                parentSplit->ratios.insert(parentSplit->ratios.begin() + static_cast<std::ptrdiff_t>(insertAt),
                                           base * 0.5f);
                NormalizeRatios(*parentSplit);
                return true;
            }

            for (auto &child : parentSplit->children) {
                if (child.get() != target) {
                    continue;
                }
                auto sub = std::make_unique<SplitNode>();
                sub->orientation = want;
                sub->ratios = {0.5f, 0.5f};
                LayoutNodePtr targetOwned = std::move(child);
                if (before) {
                    sub->children.push_back(std::move(newTab));
                    sub->children.push_back(std::move(targetOwned));
                } else {
                    sub->children.push_back(std::move(targetOwned));
                    sub->children.push_back(std::move(newTab));
                }
                child = std::move(sub);
                NormalizeRatios(*parentSplit);
                return true;
            }
            return false;
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
        floatingPanels.clear();

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
        floatingPanels.clear();
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
        TabNode *target = FindTab(targetPanelId);
        if (target == nullptr) {
            return false;
        }

        TabNode *source = FindTab(panelId);
        if (source == nullptr) {
            // Panel is not docked yet: add it to the target tab (e.g. building a
            // default layout by grouping a registered-but-unplaced panel).
            target->panels.push_back(PanelNode{panelId});
            target->activeIndex = static_cast<int32_t>(target->panels.size()) - 1;
            return true;
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

    bool LayoutModel::DockPanel(const std::string &panelId, const std::string &targetPanelId, DockPosition position)
    {
        if (panelId == targetPanelId || FindTab(targetPanelId) == nullptr) {
            return false;
        }
        const bool docked = FindTab(panelId) != nullptr;
        const bool floating = IsFloating(panelId);
        if (!docked && !floating) {
            return false;
        }

        if (docked) {
            RemovePanelFromTab(FindTab(panelId), panelId);
        }
        if (floating) {
            floatingPanels.erase(std::remove_if(floatingPanels.begin(), floatingPanels.end(),
                                                [&](const FloatingPanel &fp) { return fp.panelId == panelId; }),
                                 floatingPanels.end());
        }
        Collapse(root);

        TabNode *target = FindTab(targetPanelId);
        if (target == nullptr) {
            return false;
        }

        if (position == DockPosition::CENTER) {
            const auto targetIt = std::find_if(target->panels.begin(), target->panels.end(),
                                               [&](const PanelNode &panel) { return panel.panelId == targetPanelId; });
            const size_t at = targetIt == target->panels.end()
                ? target->panels.size()
                : static_cast<size_t>(targetIt - target->panels.begin()) + 1;
            target->panels.insert(target->panels.begin() + static_cast<std::ptrdiff_t>(at), PanelNode{panelId});
            target->activeIndex = static_cast<int32_t>(at);
            return true;
        }

        auto newTab = std::make_unique<TabNode>();
        newTab->panels.push_back(PanelNode{panelId});
        newTab->activeIndex = 0;
        return InsertBeside(root, target, std::move(newTab), position);
    }

    bool LayoutModel::FloatPanel(const std::string &panelId, const FloatingPanel &geometry)
    {
        TabNode *tab = FindTab(panelId);
        if (tab == nullptr || IsFloating(panelId)) {
            return false;
        }
        RemovePanelFromTab(tab, panelId);
        Collapse(root);

        FloatingPanel entry = geometry;
        entry.panelId = panelId;
        floatingPanels.push_back(entry);
        return true;
    }

    bool LayoutModel::DockFloatingPanel(const std::string &panelId, const std::string &targetPanelId, DockPosition position)
    {
        if (!IsFloating(panelId)) {
            return false;
        }
        return DockPanel(panelId, targetPanelId, position);
    }

    bool LayoutModel::SetFloatingGeometry(const std::string &panelId, float x, float y, float width, float height)
    {
        for (auto &fp : floatingPanels) {
            if (fp.panelId == panelId) {
                fp.x = x;
                fp.y = y;
                fp.width = width;
                fp.height = height;
                return true;
            }
        }
        return false;
    }

    bool LayoutModel::IsFloating(const std::string &panelId) const
    {
        return std::any_of(floatingPanels.begin(), floatingPanels.end(),
                           [&](const FloatingPanel &fp) { return fp.panelId == panelId; });
    }

    const FloatingPanel *LayoutModel::FindFloating(const std::string &panelId) const
    {
        for (const auto &fp : floatingPanels) {
            if (fp.panelId == panelId) {
                return &fp;
            }
        }
        return nullptr;
    }

    void LayoutModel::CollectFloating(std::vector<std::string> &out) const
    {
        for (const auto &fp : floatingPanels) {
            out.push_back(fp.panelId);
        }
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
        if (!floatingPanels.empty()) {
            rapidjson::Value floating(rapidjson::kArrayType);
            for (const auto &fp : floatingPanels) {
                rapidjson::Value entry(rapidjson::kObjectType);
                entry.AddMember("panel", rapidjson::StringRef(fp.panelId.c_str(),
                                                             static_cast<rapidjson::SizeType>(fp.panelId.size())),
                                allocator);
                entry.AddMember("x", fp.x, allocator);
                entry.AddMember("y", fp.y, allocator);
                entry.AddMember("w", fp.width, allocator);
                entry.AddMember("h", fp.height, allocator);
                entry.AddMember("active", fp.active, allocator);
                floating.PushBack(entry, allocator);
            }
            document.AddMember("floating", floating, allocator);
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

        out.floatingPanels.clear();
        const auto floatingIt = document.FindMember("floating");
        if (floatingIt != document.MemberEnd() && floatingIt->value.IsArray()) {
            for (const auto &entry : floatingIt->value.GetArray()) {
                if (!entry.IsObject()) {
                    continue;
                }
                const auto panelIt = entry.FindMember("panel");
                if (panelIt == entry.MemberEnd() || !panelIt->value.IsString()) {
                    continue;
                }
                const std::string id = panelIt->value.GetString();
                if (registry != nullptr && !registry->Contains(id)) {
                    if (warnings != nullptr) {
                        warnings->push_back("unknown panel: " + id);
                    }
                    continue;
                }
                const auto readFloat = [&entry](const char *key, float fallback) -> float {
                    const auto it = entry.FindMember(key);
                    return (it != entry.MemberEnd() && it->value.IsNumber()) ? it->value.GetFloat() : fallback;
                };
                FloatingPanel fp;
                fp.panelId = id;
                fp.x = readFloat("x", 0.0f);
                fp.y = readFloat("y", 0.0f);
                fp.width = readFloat("w", 0.0f);
                fp.height = readFloat("h", 0.0f);
                const auto activeIt = entry.FindMember("active");
                fp.active = activeIt != entry.MemberEnd() && activeIt->value.IsBool() && activeIt->value.GetBool();
                out.floatingPanels.push_back(fp);
            }
        }
        return true;
    }

} // namespace sky::editor

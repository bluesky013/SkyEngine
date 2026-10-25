#include <editor/widgets/GraphCanvas.h>

#include <algorithm>
#include <cmath>
#include <editor/ui/UiDraw.h>
#include <set>
#include <ui/UIPaintContext.h>

namespace sky::editor {
    using namespace sky::ui;
    namespace {
        bool Finite(UIPoint value)
        {
            return std::isfinite(value.x) && std::isfinite(value.y);
        }
        UIRect Between(UIPoint a, UIPoint b)
        {
            return {std::min(a.x, b.x), std::min(a.y, b.y), std::max(a.x, b.x), std::max(a.y, b.y)};
        }
        void Line(UIPaintContext &context, UIPoint a, UIPoint b, float width, uint32_t color)
        {
            const float length = std::hypot(b.x - a.x, b.y - a.y);
            if (length == 0)
                return;
            const float x = (a.y - b.y) * width / (2 * length), y = (b.x - a.x) * width / (2 * length);
            context.AddTriangle(a.x + x, a.y + y, a.x - x, a.y - y, b.x + x, b.y + y, color);
            context.AddTriangle(b.x + x, b.y + y, a.x - x, a.y - y, b.x - x, b.y - y, color);
        }
    } // namespace
    GraphCanvas::GraphCanvas(UITextSystem *text) : skin(GetDefaultUiTheme(), text)
    {
        SetFocusable(true);
        SetLayoutVolatile(false);
    }
    bool GraphCanvas::SetModel(GraphCanvasModel value)
    {
        if (value.nodes.size() > 4096 || value.links.size() > 8192)
            return false;
        std::set<Uuid> ids;
        for (const auto &node : value.nodes) {
            if (!node.id || !ids.insert(node.id).second || !Finite(node.position) || node.pins.size() > 64)
                return false;
            std::set<uint32_t> pins;
            for (const auto &pin : node.pins)
                if (!pins.insert(pin.id).second || pin.type.Empty())
                    return false;
        }
        CancelInteraction();
        model = std::move(value);
        std::erase_if(selection, [&](const auto &id) { return !ids.contains(id); });
        MarkPaintDirty();
        return true;
    }
    bool GraphCanvas::SetView(GraphCanvasView value)
    {
        if (!Finite(value.pan) || !std::isfinite(value.zoom) || value.zoom < .25f || value.zoom > 2.5f)
            return false;
        CancelInteraction();
        view = value;
        MarkPaintDirty();
        return true;
    }
    void GraphCanvas::SetActive(std::span<const Uuid> ids)
    {
        for (auto &node : model.nodes)
            node.active = std::ranges::find(ids, node.id) != ids.end();
        MarkPaintDirty();
    }
    bool GraphCanvas::FrameAll()
    {
        if (model.nodes.empty())
            return true;
        auto bounds = NodeRect(model.nodes.front());
        for (const auto &node : model.nodes) {
            const auto rect = NodeRect(node);
            bounds.left     = std::min(bounds.left, rect.left);
            bounds.top      = std::min(bounds.top, rect.top);
            bounds.right    = std::max(bounds.right, rect.right);
            bounds.bottom   = std::max(bounds.bottom, rect.bottom);
        }
        const float scale = skin.Theme().scale;
        const float pad   = skin.Theme().metrics.rowHeight / scale;
        const float width = GetBounds().Width() / scale, height = GetBounds().Height() / scale;
        if (!std::isfinite(bounds.Width()) || !std::isfinite(bounds.Height()) || bounds.Width() <= 0 || bounds.Height() <= 0 || width <= 2 * pad ||
            height <= 2 * pad)
            return false;
        GraphCanvasView next;
        next.zoom = std::clamp(std::min((width - 2 * pad) / bounds.Width(), (height - 2 * pad) / bounds.Height()), .25f, 1.f);
        next.pan  = {(width - bounds.Width() * next.zoom) * .5f - bounds.left * next.zoom,
                     (height - bounds.Height() * next.zoom) * .5f - bounds.top * next.zoom};
        if (!SetView(next))
            return false;
        const auto callback = callbacks.viewChanged;
        if (callback)
            callback(next);
        return true;
    }
    void GraphCanvas::SetSelection(std::span<const Uuid> ids)
    {
        std::vector<Uuid> next;
        for (const auto &id : ids)
            if (FindNode(id) && std::ranges::find(next, id) == next.end())
                next.push_back(id);
        selection = std::move(next);
        MarkPaintDirty();
    }
    UIPoint GraphCanvas::ToGraph(UIPoint point) const
    {
        const float scale = skin.Theme().scale;
        return {((point.x - GetBounds().left) / scale - view.pan.x) / view.zoom, ((point.y - GetBounds().top) / scale - view.pan.y) / view.zoom};
    }
    UIPoint GraphCanvas::ToScreen(UIPoint point) const
    {
        const float scale = skin.Theme().scale;
        return {(point.x * view.zoom + view.pan.x) * scale + GetBounds().left, (point.y * view.zoom + view.pan.y) * scale + GetBounds().top};
    }
    UIRect GraphCanvas::NodeRect(const GraphCanvasNode &node) const
    {
        const auto &m      = skin.Theme().metrics;
        const auto  inputs = std::ranges::count_if(node.pins, [](const auto &pin) { return !pin.output; });
        const auto  rows   = std::max(size_t(inputs), node.pins.size() - inputs);
        const float scale  = skin.Theme().scale;
        return {node.position.x, node.position.y, node.position.x + m.fieldWidth / scale,
                node.position.y + (m.panelHeaderHeight + m.rowHeight * std::max(size_t{1}, rows) + m.controlPad) / scale};
    }
    UIPoint GraphCanvas::PinPoint(const GraphCanvasNode &node, const GraphCanvasPin &pin) const
    {
        uint32_t row = 0;
        for (const auto &candidate : node.pins) {
            if (candidate.id == pin.id)
                break;
            if (candidate.output == pin.output)
                ++row;
        }
        const auto  rect = NodeRect(node);
        const auto &m    = skin.Theme().metrics;
        return {pin.output ? rect.right : rect.left, rect.top + (m.panelHeaderHeight + m.rowHeight * (row + .5f)) / skin.Theme().scale};
    }
    const GraphCanvasNode *GraphCanvas::FindNode(const Uuid &id) const
    {
        const auto found = std::ranges::find(model.nodes, id, &GraphCanvasNode::id);
        return found == model.nodes.end() ? nullptr : &*found;
    }
    const GraphCanvasPin *GraphCanvas::FindPin(const GraphCanvasEndpoint &endpoint) const
    {
        const auto *node = FindNode(endpoint.node);
        if (!node)
            return nullptr;
        const auto found = std::ranges::find(node->pins, endpoint.pin, &GraphCanvasPin::id);
        return found == node->pins.end() ? nullptr : &*found;
    }
    std::optional<GraphCanvasEndpoint> GraphCanvas::HitPin(UIPoint screen) const
    {
        const auto point  = ToGraph(screen);
        const auto radius = skin.Theme().metrics.selectionMarkerRadius / skin.Theme().scale;
        for (auto it = model.nodes.rbegin(); it != model.nodes.rend(); ++it)
            for (const auto &pin : it->pins) {
                const auto location = PinPoint(*it, pin);
                if (std::hypot(point.x - location.x, point.y - location.y) <= radius)
                    return GraphCanvasEndpoint{it->id, pin.id};
            }
        return {};
    }
    Uuid GraphCanvas::HitNode(UIPoint screen) const
    {
        const auto point = ToGraph(screen);
        for (auto it = model.nodes.rbegin(); it != model.nodes.rend(); ++it)
            if (NodeRect(*it).Contains(point.x, point.y))
                return it->id;
        return {};
    }
    void GraphCanvas::CancelInteraction()
    {
        if (gesture == Gesture::Move)
            for (const auto &old : beforeMove) {
                const auto node = std::ranges::find(model.nodes, old.node, &GraphCanvasNode::id);
                if (node != model.nodes.end())
                    node->position = old.position;
            }
        if (gesture == Gesture::Pan)
            view = beforeView;
        gesture = Gesture::None;
        beforeMove.clear();
        MarkPaintDirty();
    }
    void GraphCanvas::OnPointerCancel(uint32_t pointerId)
    {
        if (pointerId == pointer)
            CancelInteraction();
    }
    UIEventResult GraphCanvas::OnKeyEvent(const UIKeyEvent &event)
    {
        if (event.keyCode == 27 && event.action == UIKeyAction::DOWN && gesture != Gesture::None) {
            CancelInteraction();
            return UIEventResult::HANDLED;
        }
        return UIEventResult::UNHANDLED;
    }
    UIEventResult GraphCanvas::OnPointerEvent(const UIPointerEvent &event)
    {
        if (!Finite({event.x, event.y}))
            return UIEventResult::UNHANDLED;
        const UIPoint screen{event.x, event.y};
        if (event.action == UIPointerAction::WHEEL) {
            if (gesture != Gesture::None || !std::isfinite(event.wheelDelta))
                return UIEventResult::UNHANDLED;
            const auto anchor = ToGraph(screen);
            view.zoom         = std::clamp(view.zoom * std::pow(1.1f, std::clamp(event.wheelDelta, -10.f, 10.f)), .25f, 2.5f);
            view.pan          = {(screen.x - GetBounds().left) / skin.Theme().scale - anchor.x * view.zoom,
                                 (screen.y - GetBounds().top) / skin.Theme().scale - anchor.y * view.zoom};
            MarkPaintDirty();
            const auto callback = callbacks.viewChanged;
            if (callback)
                callback(view);
            return UIEventResult::HANDLED;
        }
        if (event.action == UIPointerAction::DOWN) {
            if (gesture != Gesture::None)
                return UIEventResult::UNHANDLED;
            pointer = event.pointerId;
            start = cursor = ToGraph(screen);
            if (event.button != 0) {
                gesture    = Gesture::Pan;
                beforeView = view;
                start      = screen;
            } else if (const auto hit = HitPin(screen)) {
                connection = *hit;
                gesture    = Gesture::Connect;
            } else if (const auto node = HitNode(screen)) {
                if (std::ranges::find(selection, node) == selection.end())
                    selection = {node};
                beforeMove.clear();
                for (const auto &id : selection)
                    if (const auto *value = FindNode(id))
                        beforeMove.push_back({id, value->position});
                gesture = Gesture::Move;
                MarkPaintDirty();
                const auto callback = callbacks.selection;
                if (callback)
                    callback(selection);
                return UIEventResult::HANDLED;
            } else {
                gesture = Gesture::Select;
                selection.clear();
            }
            MarkPaintDirty();
            return UIEventResult::HANDLED;
        }
        if (gesture == Gesture::None || pointer != event.pointerId)
            return UIEventResult::UNHANDLED;
        cursor = ToGraph(screen);
        if (gesture == Gesture::Pan)
            view.pan = {beforeView.pan.x + (screen.x - start.x) / skin.Theme().scale, beforeView.pan.y + (screen.y - start.y) / skin.Theme().scale};
        if (gesture == Gesture::Move)
            for (const auto &old : beforeMove) {
                const auto node = std::ranges::find(model.nodes, old.node, &GraphCanvasNode::id);
                if (node != model.nodes.end())
                    node->position = {old.position.x + cursor.x - start.x, old.position.y + cursor.y - start.y};
            }
        MarkPaintDirty();
        if (event.action != UIPointerAction::UP)
            return UIEventResult::HANDLED;
        const auto completed = gesture;
        gesture              = Gesture::None;
        if (completed == Gesture::Move) {
            std::vector<GraphCanvasPosition> moved;
            for (const auto &old : beforeMove)
                if (const auto *node = FindNode(old.node))
                    moved.push_back({node->id, node->position});
            beforeMove.clear();
            const auto callback = callbacks.moved;
            if (callback)
                callback(std::move(moved));
        } else if (completed == Gesture::Select) {
            const auto box = Between(start, cursor);
            for (const auto &node : model.nodes)
                if (!UIRect::Intersect(box, NodeRect(node)).IsEmpty())
                    selection.push_back(node.id);
            const auto callback = callbacks.selection;
            if (callback)
                callback(selection);
        } else if (completed == Gesture::Connect) {
            const auto  target = HitPin(screen);
            const auto *from   = FindPin(connection);
            const auto *to     = target ? FindPin(*target) : nullptr;
            if (from && to && from->output != to->output && from->type == to->type && target->node != connection.node) {
                const auto            callback = callbacks.connected;
                const GraphCanvasLink link     = from->output ? GraphCanvasLink{connection, *target} : GraphCanvasLink{*target, connection};
                if (callback)
                    callback(link);
            }
        } else if (completed == Gesture::Pan) {
            const auto callback = callbacks.viewChanged;
            if (callback)
                callback(view);
        }
        return UIEventResult::HANDLED;
    }
    void GraphCanvas::OnPaint(UIPaintContext &context)
    {
        auto        theme = skin.Theme();
        const float scale = theme.scale;
        theme.metrics.Scale(1.f / scale);
        for (auto *font : {&theme.fonts.banner, &theme.fonts.title, &theme.fonts.section, &theme.fonts.label, &theme.fonts.value, &theme.fonts.small,
                           &theme.fonts.tiny})
            *font = std::max(1u, static_cast<uint32_t>(*font / scale + .5f));
        const UiSkin graphSkin(theme, skin.Text());
        const auto  &m = theme.metrics;
        skin.DrawPanel(context, GetBounds(), {}, false);
        context.PushClip(GetBounds());
        context.PushTransform(
            {view.zoom * scale, 0, GetBounds().left + view.pan.x * scale, 0, view.zoom * scale, GetBounds().top + view.pan.y * scale});
        for (const auto &link : model.links) {
            const auto *a = FindNode(link.from.node), *b = FindNode(link.to.node);
            const auto *pa = FindPin(link.from), *pb = FindPin(link.to);
            if (a && b && pa && pb)
                Line(context, PinPoint(*a, *pa), PinPoint(*b, *pb), m.selectionWidth, theme.colors.accent);
        }
        for (const auto &node : model.nodes) {
            const auto rect = NodeRect(node);
            graphSkin.DrawPanel(context, rect, node.label);
            if (node.active || node.invalid || std::ranges::find(selection, node.id) != selection.end())
                uidraw::Border(context, rect, node.invalid ? theme.colors.error : theme.colors.accentHover, m.selectionWidth);
            for (const auto &pin : node.pins) {
                const auto point  = PinPoint(node, pin);
                const auto radius = m.selectionMarkerRadius;
                graphSkin.DrawField(context, {point.x - radius, point.y - radius, point.x + radius, point.y + radius}, true, false);
                const UIRect text{rect.left + m.controlPad, point.y - m.rowHeight * .5f, rect.right - m.controlPad, point.y + m.rowHeight * .5f};
                uidraw::Text(context, pin.label, theme.fonts.label, text, theme.colors.text, skin.Text(),
                             pin.output ? uidraw::HAlign::Right : uidraw::HAlign::Left);
            }
        }
        if (gesture == Gesture::Select)
            uidraw::Border(context, Between(start, cursor), theme.colors.accent, m.selectionWidth);
        if (gesture == Gesture::Connect) {
            const auto *node = FindNode(connection.node);
            const auto *pin  = FindPin(connection);
            if (node && pin)
                Line(context, PinPoint(*node, *pin), cursor, m.selectionWidth, theme.colors.accentHover);
        }
        context.PopTransform();
        context.PopClip();
    }
} // namespace sky::editor

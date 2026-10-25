#pragma once

#include <core/name/Name.h>
#include <core/util/Uuid.h>
#include <editor/widgets/UiSkin.h>
#include <functional>
#include <optional>
#include <span>
#include <ui/UIElement.h>

namespace sky::editor {
    struct GraphCanvasPin {
        uint32_t    id = 0;
        std::string label;
        Name        type;
        bool        output = false;
    };
    struct GraphCanvasNode {
        Uuid                        id;
        std::string                 label;
        sky::ui::UIPoint            position;
        std::vector<GraphCanvasPin> pins;
        bool                        active = false, invalid = false;
    };
    struct GraphCanvasEndpoint {
        Uuid     node;
        uint32_t pin                                           = 0;
        bool     operator==(const GraphCanvasEndpoint &) const = default;
    };
    struct GraphCanvasLink {
        GraphCanvasEndpoint from, to;
    };
    struct GraphCanvasPosition {
        Uuid             node;
        sky::ui::UIPoint position;
    };
    struct GraphCanvasModel {
        std::vector<GraphCanvasNode> nodes;
        std::vector<GraphCanvasLink> links;
    };
    struct GraphCanvasView {
        sky::ui::UIPoint pan;
        float            zoom = 1.f;
    };
    struct GraphCanvasCallbacks {
        std::function<void(std::vector<Uuid>)>                selection;
        std::function<void(std::vector<GraphCanvasPosition>)> moved;
        std::function<void(GraphCanvasLink)>                  connected;
        std::function<void(GraphCanvasView)>                  viewChanged;
    };

    class GraphCanvas final : public sky::ui::UIElement {
    public:
        explicit GraphCanvas(sky::ui::UITextSystem *text);
        bool SetModel(GraphCanvasModel value);
        bool SetView(GraphCanvasView value);
        bool FrameAll();
        void SetCallbacks(GraphCanvasCallbacks value)
        {
            callbacks = std::move(value);
        }
        void                     SetSelection(std::span<const Uuid> ids);
        void                     SetActive(std::span<const Uuid> ids);
        const std::vector<Uuid> &Selection() const
        {
            return selection;
        }
        const GraphCanvasModel &Model() const
        {
            return model;
        }
        GraphCanvasView View() const
        {
            return view;
        }
        sky::ui::UIPoint                   ToGraph(sky::ui::UIPoint point) const;
        sky::ui::UIPoint                   ToScreen(sky::ui::UIPoint point) const;
        std::optional<GraphCanvasEndpoint> HitPin(sky::ui::UIPoint screen) const;
        Uuid                               HitNode(sky::ui::UIPoint screen) const;
        void                               CancelInteraction();
        bool                               WantsPointerCapture() const override
        {
            return gesture != Gesture::None;
        }
        void                   OnPaint(sky::ui::UIPaintContext &context) override;
        sky::ui::UIEventResult OnPointerEvent(const sky::ui::UIPointerEvent &event) override;
        void                   OnPointerCancel(uint32_t pointerId) override;
        void                   OnPointerButtonConflict(uint32_t pointerId) override
        {
            OnPointerCancel(pointerId);
        }
        sky::ui::UIEventResult OnKeyEvent(const sky::ui::UIKeyEvent &event) override;

    private:
        enum class Gesture { None, Move, Pan, Connect, Select };
        sky::ui::UIRect                  NodeRect(const GraphCanvasNode &node) const;
        sky::ui::UIPoint                 PinPoint(const GraphCanvasNode &node, const GraphCanvasPin &pin) const;
        const GraphCanvasNode           *FindNode(const Uuid &id) const;
        const GraphCanvasPin            *FindPin(const GraphCanvasEndpoint &endpoint) const;
        UiSkin                           skin;
        GraphCanvasModel                 model;
        GraphCanvasView                  view, beforeView;
        GraphCanvasCallbacks             callbacks;
        std::vector<Uuid>                selection;
        std::vector<GraphCanvasPosition> beforeMove;
        GraphCanvasEndpoint              connection;
        sky::ui::UIPoint                 start, cursor;
        Gesture                          gesture = Gesture::None;
        uint32_t                         pointer = 0;
    };
} // namespace sky::editor

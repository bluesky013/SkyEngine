//
// ChildRouting implementation (see ChildRouting.h).
//

#include <ui/detail/ChildRouting.h>

namespace sky::ui {

    UIEventResult RouteChildPointer(const std::vector<UIElementPtr> &children, UIElement *&captured, const UIPointerEvent &event, bool topmostFirst)
    {
        if (captured != nullptr) {
            const UIEventResult result = captured->OnPointerEvent(event);
            if (event.action == UIPointerAction::UP) {
                captured = nullptr;
            }
            return result;
        }

        const size_t count = children.size();
        for (size_t k = 0; k < count; ++k) {
            const size_t index   = topmostFirst ? (count - 1 - k) : k;
            UIElement   *element = children[index].get();
            if (element == nullptr || !element->IsVisible()) {
                continue;
            }
            if (!element->GetBounds().Contains(event.x, event.y)) {
                continue;
            }
            if (event.action == UIPointerAction::DOWN) {
                captured = element;
            }
            return element->OnPointerEvent(event);
        }
        return UIEventResult::UNHANDLED;
    }

} // namespace sky::ui

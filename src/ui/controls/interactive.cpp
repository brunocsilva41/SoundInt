#include "ui/controls/interactive.h"

namespace soundint::ui::controls {

// ---------------------------------------------------------------------------
// Estado visual compartilhado (hover / pressao / foco)
// ---------------------------------------------------------------------------
bool InteractiveControl::updateHover(float x, float y)
{
    const bool inside = bounds_.contains(x, y);
    if (inside == hovered_) {
        return false;
    }
    hovered_ = inside;
    return true;
}

void InteractiveControl::renderOverlay(IRenderTarget& rt, const tokens::Palette& palette,
                                       const Rect& rect) const
{
    if (!enabled_) {
        return;
    }
    if (pressed_) {
        rt.fillRoundedRect(rect, tokens::kRadiusMd, palette.pressed);
    } else if (hovered_) {
        rt.fillRoundedRect(rect, tokens::kRadiusMd, palette.hover);
    }
}

void InteractiveControl::renderFocus(IRenderTarget& rt, const tokens::Palette& palette,
                                     const Rect& rect) const
{
    if (focused_) {
        rt.strokeRoundedRect(rect, tokens::kRadiusMd, 1.5f, palette.accent);
    }
}

}  // namespace soundint::ui::controls

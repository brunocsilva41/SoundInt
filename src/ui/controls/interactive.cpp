#include "ui/controls/interactive.h"

#include "ui/controls/metrics.h"

namespace soundint::ui::controls {

// ---------------------------------------------------------------------------
// Estado visual compartilhado (hover / pressao / foco). Todas as metricas
// (raio, espessura) reagem ao fator DPI do controle via scale_.
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
    const float radius = metrics::radius(tokens::kRadiusMd, scale_);
    if (pressed_) {
        rt.fillRoundedRect(rect, radius, palette.pressed);
    } else if (hovered_) {
        rt.fillRoundedRect(rect, radius, palette.hover);
    }
}

void InteractiveControl::renderFocus(IRenderTarget& rt, const tokens::Palette& palette,
                                     const Rect& rect) const
{
    if (focused_) {
        rt.strokeRoundedRect(rect, metrics::radius(tokens::kRadiusMd, scale_),
                             metrics::stroke(scale_), palette.accent);
    }
}

}  // namespace soundint::ui::controls

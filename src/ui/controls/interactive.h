// ============================================================================
// Base comum dos controles interativos (hover, pressao, foco e captura).
// Track E — somente IRenderTarget + tokens:: (nunca renderer_d2d/palette.h).
// ============================================================================
#pragma once

#include "ui/control.h"

namespace soundint::ui::controls {

// Roteamento esperado da janela (Wave 2):
//   - Move -> controle sob o cursor;
//   - Down -> o controle que consumir o Down recebe Move/Up ate o Up/Leave
//     (captura). Coordenadas em pixels do cliente, relativas a janela.
class InteractiveControl : public Control {
public:
    void setFocused(bool focused) { focused_ = focused; }
    bool focused() const { return focused_; }
    bool hovered() const { return hovered_; }
    bool pressed() const { return pressed_; }

protected:
    // Atualiza hovered_ pela coordenada. Retorna true se o estado mudou.
    bool updateHover(float x, float y);

    // Overlay palette.hover/pressed sobre o retangulo dado.
    void renderOverlay(IRenderTarget& rt, const tokens::Palette& palette,
                       const Rect& rect) const;
    void renderOverlay(IRenderTarget& rt, const tokens::Palette& palette) const
    {
        renderOverlay(rt, palette, bounds_);
    }

    // Borda de foco: accent 1.5px.
    void renderFocus(IRenderTarget& rt, const tokens::Palette& palette,
                     const Rect& rect) const;
    void renderFocus(IRenderTarget& rt, const tokens::Palette& palette) const
    {
        renderFocus(rt, palette, bounds_);
    }

    bool hovered_ = false;
    bool pressed_ = false;
    bool focused_ = false;
    bool captured_ = false;   // Down consumido; aguarda Up
};

}  // namespace soundint::ui::controls

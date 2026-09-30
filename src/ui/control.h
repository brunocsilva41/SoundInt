// ============================================================================
// CONTRACT — base de controles visuais. Header-only, CONGELADO na Wave 0.
// Track E implementa controles concretos; janelas (Wave 2) compoem.
// ============================================================================
#pragma once

#include "ui/design_tokens.h"
#include "ui/renderer.h"

#include <string>

namespace soundint::ui {

class Control {
public:
    virtual ~Control() = default;

    const Rect& bounds() const { return bounds_; }
    void setBounds(const Rect& r) { bounds_ = r; }

    // Desenho. A janela ja preparou o IRenderTarget e resolve a paleta.
    virtual void render(IRenderTarget& rt, const tokens::Palette& palette) = 0;

    // Retornar true = evento consumido. Coordenadas em pixels do cliente.
    virtual bool onPointer(const PointerEvent&) { return false; }
    virtual bool onKey(const KeyEvent&) { return false; }

    // Altura natural desejada para a largura dada (layouts).
    virtual Size measure(float /*constraintWidth*/)
    {
        return {bounds_.w, bounds_.h};
    }

    void setEnabled(bool enabled) { enabled_ = enabled; }
    bool enabled() const { return enabled_; }
    void setVisible(bool visible) { visible_ = visible; }
    bool visible() const { return visible_; }

    // Nome acessivel (leitores de tela) — texto alternativo.
    void setAccessibleName(std::wstring name) { accessibleName_ = std::move(name); }
    const std::wstring& accessibleName() const { return accessibleName_; }

    // Marca a janela pai como suja (usa o handler global de requestRender).
    void invalidate();

protected:
    Rect bounds_{};
    bool enabled_ = true;
    bool visible_ = true;
    std::wstring accessibleName_;
};

}  // namespace soundint::ui

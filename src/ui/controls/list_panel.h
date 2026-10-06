// ============================================================================
// ListPanel: empilhamento vertical com scrollY.
// NAO possui os filhos — a janela pai mantem a posse (ponteiro bruto).
// layout() distribui os controles visiveis em cascata via setBounds;
// render() chama layout(), aplica pushClip(bounds_) e desenha os filhos.
// Wheel (dentro dos bounds) ajusta scrollY em kScrollStep por notch
// (cima = diminui o offset, mostrando o inicio da lista).
// DPI: gap (px logicos) e passo de scroll escalam com scale(); as alturas
// vem de measure() dos filhos, cada um com seu proprio scale().
// ============================================================================
#pragma once

#include "ui/control.h"

#include <vector>

namespace soundint::ui::controls {

class ListPanel : public Control {
public:
    static constexpr float kScrollStep = tokens::kRowHeight;

    void addControl(Control* control);   // nao possui; ignora ponteiro nulo
    void clear();                        // remove da lista (nao deleta)
    const std::vector<Control*>& children() const { return children_; }

    // Gap entre filhos em px LOGICOS (efetivo = gap * scale()).
    void setGap(float gap) { gap_ = gap; }
    float gap() const { return gap_; }

    void setScrollY(float scrollY);
    float scrollY() const { return scrollY_; }
    float contentHeight() const;   // soma das alturas + gaps
    float maxScrollY() const;      // contentHeight - viewport (>= 0)

    // setBounds em cascata; chamado por render()/onPointer() tambem.
    void layout();

    void render(IRenderTarget& rt, const tokens::Palette& palette) override;
    bool onPointer(const PointerEvent& e) override;
    bool onKey(const KeyEvent& e) override;
    Size measure(float constraintWidth) override;

private:
    float effectiveGap() const;   // gap_ * scale_ (px fisicos)
    float childHeight(Control& child) const;

    std::vector<Control*> children_;
    float scrollY_ = 0.f;
    float gap_ = tokens::kSpace1;
};

}  // namespace soundint::ui::controls

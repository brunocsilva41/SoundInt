// ============================================================================
// Toggle (switch) com valor bool e notificacao onChanged.
// Sem timer: o estado alterna de imediato (binario); a posicao do pino e
// derivada puramente do valor em knobRect(). Trilha visual de 48x24 px
// logicos, thumb/track proporcionais a scale().
// Pointer: Down captura; Up dentro -> alterna. Teclado: Enter/Space focado.
// ============================================================================
#pragma once

#include "ui/controls/interactive.h"

#include <functional>

namespace soundint::ui::controls {

class Toggle : public InteractiveControl {
public:
    using ChangedFn = std::function<void(bool)>;

    // Geometria pura (exposta para teste). scale = fator DPI (default 1.0).
    static Rect trackRect(const Rect& bounds, float scale = 1.f);
    static Rect knobRect(const Rect& bounds, bool on, float scale = 1.f);

    // Set programatico: nao dispara onChanged (evita loops com a janela).
    void setValue(bool value) { value_ = value; }
    bool value() const { return value_; }
    void setOnChanged(ChangedFn fn) { onChanged_ = std::move(fn); }

    Rect knobRect() const { return knobRect(bounds_, value_, scale_); }

    void render(IRenderTarget& rt, const tokens::Palette& palette) override;
    bool onPointer(const PointerEvent& e) override;
    bool onKey(const KeyEvent& e) override;
    Size measure(float constraintWidth) override;

private:
    void toggle();

    bool value_ = false;
    ChangedFn onChanged_;
};

}  // namespace soundint::ui::controls

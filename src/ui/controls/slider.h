// ============================================================================
// Slider horizontal: valor normalizado 0..1 com clamp.
// Pointer: Down captura -> Move atualiza; wheel +-0.05 por notch; setas
// +-0.01 (Shift +-0.1). Trilho + pino em palette.accent.
// A logica pura valueFromX()/xFromValue() e estatica (testavel sem desenho).
// ============================================================================
#pragma once

#include "ui/controls/interactive.h"

#include <functional>

namespace soundint::ui::controls {

class Slider : public InteractiveControl {
public:
    using ChangedFn = std::function<void(float)>;

    // Logica pura (exposta para teste): mapeia x <-> valor com clamp 0..1.
    static float valueFromX(float x, const Rect& bounds);
    static float xFromValue(float value, const Rect& bounds);

    // Set programatico: nao dispara onChanged (evita loops com a janela).
    void setValue(float value);
    float value() const { return value_; }
    void setOnChanged(ChangedFn fn) { onChanged_ = std::move(fn); }

    void render(IRenderTarget& rt, const tokens::Palette& palette) override;
    bool onPointer(const PointerEvent& e) override;
    bool onKey(const KeyEvent& e) override;
    Size measure(float constraintWidth) override;

private:
    static constexpr float kThumbD = tokens::kControlHeight / 2.f;   // 16
    static constexpr float kTrackH = tokens::kSpace2;                // 8

    void applyValue(float v);   // clampa 0..1 e notifica se mudou
    void addDelta(float delta);

    float value_ = 0.f;
    ChangedFn onChanged_;
};

}  // namespace soundint::ui::controls

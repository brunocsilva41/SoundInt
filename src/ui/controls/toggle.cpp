#include "ui/controls/toggle.h"

#include "ui/controls/metrics.h"

namespace soundint::ui::controls {

namespace {
// Dimensoes LOGICAS derivadas dos tokens (multiplicadas por scale no uso):
// trilha 48x24, pino 16 — raio kRadiusMd mantem o pino um circulo perfeito.
constexpr float kTrackH = tokens::kControlHeight - tokens::kSpace1 * 2.f;
constexpr float kTrackW = kTrackH * 2.f;
constexpr float kKnobD = kTrackH - tokens::kSpace1 * 2.f;
}  // namespace

// ---------------------------------------------------------------------------
// Geometria pura (trilha 48px logicos, proporcional ao fator DPI)
// ---------------------------------------------------------------------------
Rect Toggle::trackRect(const Rect& b, float scale)
{
    const float trackH = kTrackH * scale;
    const float trackW = kTrackW * scale;
    const float x = b.x + (b.w > trackW ? (b.w - trackW) * 0.5f : 0.f);
    const float y = b.y + (b.h > trackH ? (b.h - trackH) * 0.5f : 0.f);
    return {x, y, trackW, trackH};
}

Rect Toggle::knobRect(const Rect& b, bool on, float scale)
{
    const Rect track = trackRect(b, scale);
    const float knobD = kKnobD * scale;
    const float inset = tokens::kSpace1 * scale;
    const float travel = track.w - knobD - inset * 2.f;
    const float x = track.x + inset + (on ? travel : 0.f);
    return {x, track.y + inset, knobD, knobD};
}

// ---------------------------------------------------------------------------
// Desenho
// ---------------------------------------------------------------------------
void Toggle::render(IRenderTarget& rt, const tokens::Palette& palette)
{
    if (!visible_) {
        return;
    }

    const float radius = metrics::radius(tokens::kRadiusMd, scale_);
    const float stroke = metrics::stroke(scale_);
    const Rect track = trackRect(bounds_, scale_);

    Color trackColor = palette.surfaceAlt;
    if (enabled_ && value_) {
        trackColor = palette.accent;
    }
    rt.fillRoundedRect(track, radius, trackColor);
    rt.strokeRoundedRect(track, radius, stroke, palette.border);

    if (enabled_) {
        if (pressed_) {
            rt.fillRoundedRect(track, radius, palette.pressed);
        } else if (hovered_) {
            rt.fillRoundedRect(track, radius, palette.hover);
        }
    }

    const Rect knob = knobRect(bounds_, value_, scale_);
    rt.fillRoundedRect(knob, radius,
                       enabled_ ? palette.surface : palette.surfaceAlt);
    rt.strokeRoundedRect(knob, radius, stroke, palette.border);

    renderFocus(rt, palette, track);
}

// ---------------------------------------------------------------------------
// Entrada
// ---------------------------------------------------------------------------
bool Toggle::onPointer(const PointerEvent& e)
{
    if (!visible_) {
        return false;
    }

    switch (e.kind) {
    case PointerKind::Down:
        if (!enabled_ || e.button != MouseButton::Left || !bounds_.contains(e.x, e.y)) {
            return false;
        }
        captured_ = true;
        pressed_ = true;
        hovered_ = true;
        invalidate();
        return true;

    case PointerKind::Move:
        if (captured_) {
            pressed_ = bounds_.contains(e.x, e.y);
            invalidate();
            return true;
        }
        if (updateHover(e.x, e.y)) {
            invalidate();
        }
        return hovered_;

    case PointerKind::Up: {
        if (!captured_) {
            return false;
        }
        // Up de outro botao nao conclui a interacao (segura a captura).
        if (e.button != MouseButton::Left) {
            return true;
        }
        captured_ = false;
        pressed_ = false;
        const bool inside = bounds_.contains(e.x, e.y);
        invalidate();
        if (inside && enabled_) {
            toggle();
        }
        return true;
    }

    case PointerKind::Leave:
        hovered_ = false;
        pressed_ = false;
        invalidate();
        return false;

    default:
        return false;
    }
}

bool Toggle::onKey(const KeyEvent& e)
{
    if (!visible_ || !enabled_ || !focused_ || !e.down) {
        return false;
    }
    if (e.key == Key::Enter || e.key == Key::Space) {
        toggle();
        return true;
    }
    return false;
}

void Toggle::toggle()
{
    value_ = !value_;
    // RESILIENCIA A UAF: onChanged_ pode recriar a lista dona deste toggle
    // (rebuild sincrono). Invalidamos ANTES e nao tocamos em `this` depois
    // que o callback roda.
    invalidate();
    if (onChanged_) {
        onChanged_(value_);
    }
}

Size Toggle::measure(float constraintWidth)
{
    return {constraintWidth, tokens::kControlHeight * scale_};
}

}  // namespace soundint::ui::controls

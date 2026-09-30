#include "ui/controls/toggle.h"

namespace soundint::ui::controls {

namespace {
// Dimensoes derivadas dos tokens: trilha 48x24, pino 16 (raio = kRadiusMd,
// portanto um circulo perfeito).
constexpr float kTrackH = tokens::kControlHeight - tokens::kSpace1 * 2.f;
constexpr float kTrackW = kTrackH * 2.f;
constexpr float kKnobD = kTrackH - tokens::kSpace1 * 2.f;
}  // namespace

// ---------------------------------------------------------------------------
// Geometria pura
// ---------------------------------------------------------------------------
Rect Toggle::trackRect(const Rect& b)
{
    const float x = b.x + (b.w > kTrackW ? (b.w - kTrackW) * 0.5f : 0.f);
    const float y = b.y + (b.h > kTrackH ? (b.h - kTrackH) * 0.5f : 0.f);
    return {x, y, kTrackW, kTrackH};
}

Rect Toggle::knobRect(const Rect& b, bool on)
{
    const Rect track = trackRect(b);
    const float travel = track.w - kKnobD - tokens::kSpace1 * 2.f;
    const float x = track.x + tokens::kSpace1 + (on ? travel : 0.f);
    return {x, track.y + tokens::kSpace1, kKnobD, kKnobD};
}

// ---------------------------------------------------------------------------
// Desenho
// ---------------------------------------------------------------------------
void Toggle::render(IRenderTarget& rt, const tokens::Palette& palette)
{
    if (!visible_) {
        return;
    }

    const Rect track = trackRect(bounds_);
    Color trackColor = palette.surfaceAlt;
    if (enabled_ && value_) {
        trackColor = palette.accent;
    }
    rt.fillRoundedRect(track, tokens::kRadiusMd, trackColor);
    rt.strokeRoundedRect(track, tokens::kRadiusMd, 1.f, palette.border);

    if (enabled_) {
        if (pressed_) {
            rt.fillRoundedRect(track, tokens::kRadiusMd, palette.pressed);
        } else if (hovered_) {
            rt.fillRoundedRect(track, tokens::kRadiusMd, palette.hover);
        }
    }

    const Rect knob = knobRect(bounds_, value_);
    rt.fillRoundedRect(knob, tokens::kRadiusMd,
                       enabled_ ? palette.surface : palette.surfaceAlt);
    rt.strokeRoundedRect(knob, tokens::kRadiusMd, 1.f, palette.border);

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
    if (onChanged_) {
        onChanged_(value_);
    }
    invalidate();
}

Size Toggle::measure(float constraintWidth)
{
    return {constraintWidth, tokens::kControlHeight};
}

}  // namespace soundint::ui::controls

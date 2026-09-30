#include "ui/controls/slider.h"

namespace soundint::ui::controls {

namespace {
float clamp01(float v)
{
    if (v < 0.f) {
        return 0.f;
    }
    if (v > 1.f) {
        return 1.f;
    }
    return v;
}
}  // namespace

// ---------------------------------------------------------------------------
// Logica pura x <-> valor
// ---------------------------------------------------------------------------
float Slider::valueFromX(float x, const Rect& b)
{
    const float pad = kThumbD * 0.5f;
    const float span = b.w - pad * 2.f;
    if (span <= 0.f) {
        return 0.f;
    }
    return clamp01((x - (b.x + pad)) / span);
}

float Slider::xFromValue(float value, const Rect& b)
{
    const float pad = kThumbD * 0.5f;
    const float span = b.w - pad * 2.f;
    if (span <= 0.f) {
        return b.x + pad;
    }
    return b.x + pad + clamp01(value) * span;
}

// ---------------------------------------------------------------------------
// Valor
// ---------------------------------------------------------------------------
void Slider::setValue(float value)
{
    const float v = clamp01(value);
    if (v == value_) {
        return;
    }
    value_ = v;
    invalidate();
}

void Slider::applyValue(float v)
{
    const float c = clamp01(v);
    if (c == value_) {
        return;
    }
    value_ = c;
    if (onChanged_) {
        onChanged_(value_);
    }
    invalidate();
}

void Slider::addDelta(float delta)
{
    applyValue(value_ + delta);
}

// ---------------------------------------------------------------------------
// Desenho: trilho (surfaceAlt) + parte preenchida e pino em accent
// ---------------------------------------------------------------------------
void Slider::render(IRenderTarget& rt, const tokens::Palette& palette)
{
    if (!visible_) {
        return;
    }

    const float cy = bounds_.y + bounds_.h * 0.5f;
    const float pad = kThumbD * 0.5f;
    const float trackW = (bounds_.w > pad * 2.f) ? bounds_.w - pad * 2.f : 0.f;
    const float trackY = cy - kTrackH * 0.5f;
    const float thumbX = xFromValue(value_, bounds_);
    const Color fill = enabled_ ? palette.accent : palette.border;

    const Rect track{bounds_.x + pad, trackY, trackW, kTrackH};
    rt.fillRoundedRect(track, tokens::kRadiusSm, palette.surfaceAlt);

    const float filledW = thumbX - track.x;
    if (filledW > 0.f) {
        const Rect filled{track.x, trackY, filledW, kTrackH};
        rt.fillRoundedRect(filled, tokens::kRadiusSm, fill);
    }

    const Rect thumb{thumbX - kThumbD * 0.5f, cy - kThumbD * 0.5f, kThumbD, kThumbD};
    rt.fillRoundedRect(thumb, tokens::kRadiusMd, fill);
    rt.strokeRoundedRect(thumb, tokens::kRadiusMd, 1.5f, palette.surface);

    renderOverlay(rt, palette);
    renderFocus(rt, palette);
}

// ---------------------------------------------------------------------------
// Entrada
// ---------------------------------------------------------------------------
bool Slider::onPointer(const PointerEvent& e)
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
        applyValue(valueFromX(e.x, bounds_));
        invalidate();
        return true;

    case PointerKind::Move:
        if (captured_) {
            applyValue(valueFromX(e.x, bounds_));
            return true;
        }
        if (updateHover(e.x, e.y)) {
            invalidate();
        }
        return hovered_;

    case PointerKind::Up:
        if (!captured_) {
            return false;
        }
        captured_ = false;
        pressed_ = false;
        invalidate();
        return true;

    case PointerKind::Wheel:
        if (!enabled_ || !bounds_.contains(e.x, e.y)) {
            return false;
        }
        addDelta(0.05f * e.wheelDelta);
        return true;

    case PointerKind::Leave:
        hovered_ = false;
        pressed_ = false;
        invalidate();
        return false;

    default:
        return false;
    }
}

bool Slider::onKey(const KeyEvent& e)
{
    if (!visible_ || !enabled_ || !focused_ || !e.down) {
        return false;
    }

    const float step = e.shift ? 0.1f : 0.01f;
    switch (e.key) {
    case Key::Right:
    case Key::Up:
        addDelta(step);
        return true;
    case Key::Left:
    case Key::Down:
        addDelta(-step);
        return true;
    default:
        return false;
    }
}

Size Slider::measure(float constraintWidth)
{
    return {constraintWidth, tokens::kControlHeight};
}

}  // namespace soundint::ui::controls

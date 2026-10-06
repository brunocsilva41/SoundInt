#include "ui/controls/slider.h"

#include "ui/controls/metrics.h"

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
// Logica pura x <-> valor (thumbD em px; a instancia passa kThumbD*scale_)
// ---------------------------------------------------------------------------
float Slider::valueFromX(float x, const Rect& b, float thumbD)
{
    const float pad = thumbD * 0.5f;
    const float span = b.w - pad * 2.f;
    if (span <= 0.f) {
        return 0.f;
    }
    return clamp01((x - (b.x + pad)) / span);
}

float Slider::xFromValue(float value, const Rect& b, float thumbD)
{
    const float pad = thumbD * 0.5f;
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
    // RESILIENCIA A UAF: onChanged_ pode recriar a linha dona deste slider
    // (rebuild sincrono). Invalidamos ANTES e nao tocamos em `this` depois
    // que o callback roda.
    invalidate();
    if (onChanged_) {
        onChanged_(value_);
    }
}

void Slider::addDelta(float delta)
{
    applyValue(value_ + delta);
}

// ---------------------------------------------------------------------------
// Desenho: trilho (surfaceAlt) + parte preenchida e pino em accent; trilho e
// pino proporcionais a scale_. Hover/pressionado pintam overlay (afordancia).
// ---------------------------------------------------------------------------
void Slider::render(IRenderTarget& rt, const tokens::Palette& palette)
{
    if (!visible_) {
        return;
    }

    const float thumbD = kThumbD * scale_;
    const float trackH = kTrackH * scale_;
    const float pad = thumbD * 0.5f;
    const float cy = bounds_.y + bounds_.h * 0.5f;
    const float trackW = (bounds_.w > pad * 2.f) ? bounds_.w - pad * 2.f : 0.f;
    const float trackY = cy - trackH * 0.5f;
    const float thumbX = xFromValue(value_, bounds_, thumbD);
    const Color fill = enabled_ ? palette.accent : palette.border;

    const float radiusSm = metrics::radius(tokens::kRadiusSm, scale_);
    const float thumbR = metrics::radius(tokens::kRadiusMd, scale_);

    const Rect track{bounds_.x + pad, trackY, trackW, trackH};
    rt.fillRoundedRect(track, radiusSm, palette.surfaceAlt);

    const float filledW = thumbX - track.x;
    if (filledW > 0.f) {
        const Rect filled{track.x, trackY, filledW, trackH};
        rt.fillRoundedRect(filled, radiusSm, fill);
    }

    const Rect thumb{thumbX - pad, cy - pad, thumbD, thumbD};
    rt.fillRoundedRect(thumb, thumbR, fill);
    rt.strokeRoundedRect(thumb, thumbR, metrics::stroke(scale_), palette.surface);

    // Afordancia de hover/pressionado + anel de foco (uniforme com os demais).
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
        applyValue(valueFromX(e.x, bounds_, kThumbD * scale_));
        invalidate();
        return true;

    case PointerKind::Move:
        if (captured_) {
            applyValue(valueFromX(e.x, bounds_, kThumbD * scale_));
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
    return {constraintWidth, tokens::kControlHeight * scale_};
}

}  // namespace soundint::ui::controls

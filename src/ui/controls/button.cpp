#include "ui/controls/button.h"

namespace soundint::ui::controls {

namespace {
constexpr float kGlyphBox = 16.f;   // caixa do glifo de icone
}  // namespace

// ---------------------------------------------------------------------------
// Desenho: fundo surface + borda + overlay de estado + glifo/texto centrados
// ---------------------------------------------------------------------------
void Button::render(IRenderTarget& rt, const tokens::Palette& palette)
{
    if (!visible_) {
        return;
    }

    rt.fillRoundedRect(bounds_, tokens::kRadiusMd,
                       enabled_ ? palette.surface : palette.surfaceAlt);
    rt.strokeRoundedRect(bounds_, tokens::kRadiusMd, 1.f, palette.border);
    renderOverlay(rt, palette);

    const Color fg = enabled_ ? palette.textPrimary : palette.textSecondary;

    TextStyle icon;
    icon.fontFamily = tokens::kFontIcons;
    icon.fontSize = tokens::kFontBody;
    icon.weight = tokens::kWeightRegular;
    icon.color = fg;
    icon.align = TextStyle::Align::Center;
    icon.valign = TextStyle::VAlign::Middle;

    TextStyle ts;
    ts.fontFamily = tokens::kFontUi;
    ts.fontSize = tokens::kFontBody;
    ts.weight = tokens::kWeightRegular;
    ts.color = fg;
    ts.align = TextStyle::Align::Center;
    ts.valign = TextStyle::VAlign::Middle;

    const Size textSize = text_.empty() ? Size{} : rt.measureText(text_, ts);
    const float glyphW = glyph_.empty() ? 0.f : kGlyphBox;
    const float gap = (glyphW > 0.f && textSize.w > 0.f) ? tokens::kSpace2 : 0.f;
    const float groupW = glyphW + gap + textSize.w;
    const float innerW = bounds_.w - tokens::kSpace2 * 2.f;

    float x = bounds_.x;
    float textW = textSize.w;
    if (groupW <= innerW) {
        x = bounds_.x + (bounds_.w - groupW) * 0.5f;
    } else {
        // Texto maior que o botao: alinha a esquerda e deixa o ellipsis cortar.
        x = bounds_.x + tokens::kSpace2;
        textW = innerW - glyphW - gap;
        if (textW < 0.f) {
            textW = 0.f;
        }
    }

    if (glyphW > 0.f) {
        const Rect gbox{x, bounds_.y, kGlyphBox, bounds_.h};
        rt.drawGlyph(glyph_, gbox, icon);
    }
    if (textSize.w > 0.f) {
        const Rect tbox{x + glyphW + gap, bounds_.y, textW, bounds_.h};
        rt.drawText(text_, tbox, ts);
    }

    renderFocus(rt, palette);
}

// ---------------------------------------------------------------------------
// Entrada
// ---------------------------------------------------------------------------
bool Button::onPointer(const PointerEvent& e)
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
            click();
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

bool Button::onKey(const KeyEvent& e)
{
    if (!visible_ || !enabled_ || !focused_ || !e.down) {
        return false;
    }
    if (e.key == Key::Enter || e.key == Key::Space) {
        click();
        return true;
    }
    return false;
}

void Button::click()
{
    if (onClick_) {
        onClick_();
    }
    invalidate();
}

Size Button::measure(float constraintWidth)
{
    return {constraintWidth, tokens::kControlHeight};
}

}  // namespace soundint::ui::controls

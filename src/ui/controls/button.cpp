#include "ui/controls/button.h"

#include "ui/controls/metrics.h"

namespace soundint::ui::controls {

// ---------------------------------------------------------------------------
// Desenho: fundo surface + borda + overlay de estado + glifo/texto centrados.
// Fontes, paddings, caixa de icone (16px) e raios escalam com scale_.
// ---------------------------------------------------------------------------
void Button::render(IRenderTarget& rt, const tokens::Palette& palette)
{
    if (!visible_) {
        return;
    }

    // Fundo/borda por variante; desabilitado cai em surfaceAlt sempre.
    Color fill = palette.surface;
    Color border = palette.border;
    Color fg = palette.textPrimary;
    if (!enabled_) {
        fill = palette.surfaceAlt;
        border = palette.border;
        fg = palette.textSecondary;
    } else {
        switch (variant_) {
        case Variant::Primary:
            fill = palette.accent;
            border = palette.accent;
            fg = palette.accentFg;
            break;
        case Variant::Danger:
            fill = palette.danger;
            border = palette.danger;
            fg = palette.dangerFg;
            break;
        case Variant::Secondary:
        default:
            break;
        }
    }

    const float radius = metrics::radius(tokens::kRadiusMd, scale_);
    const float stroke = metrics::stroke(scale_);
    const float padX = tokens::kSpace2 * scale_;
    const float glyphBox = metrics::glyphBox(scale_);

    rt.fillRoundedRect(bounds_, radius, fill);
    rt.strokeRoundedRect(bounds_, radius, stroke, border);
    renderOverlay(rt, palette);

    TextStyle icon;
    icon.fontFamily = tokens::kFontIcons;
    icon.fontSize = tokens::kFontBody * scale_;
    icon.weight = tokens::kWeightRegular;
    icon.color = fg;
    icon.align = TextStyle::Align::Center;
    icon.valign = TextStyle::VAlign::Middle;

    TextStyle ts;
    ts.fontFamily = tokens::kFontUi;
    ts.fontSize = tokens::kFontBody * scale_;
    ts.weight = (variant_ == Variant::Secondary) ? tokens::kWeightRegular
                                                  : tokens::kWeightSemibold;
    ts.color = fg;
    ts.align = TextStyle::Align::Center;
    ts.valign = TextStyle::VAlign::Middle;

    const Size textSize = text_.empty() ? Size{} : rt.measureText(text_, ts);
    const float glyphW = glyph_.empty() ? 0.f : glyphBox;
    const float gap = (glyphW > 0.f && textSize.w > 0.f) ? padX : 0.f;
    const float groupW = glyphW + gap + textSize.w;
    const float innerW = bounds_.w - padX * 2.f;

    float x = bounds_.x;
    float textW = textSize.w;
    if (groupW <= innerW) {
        x = bounds_.x + (bounds_.w - groupW) * 0.5f;
    } else {
        // Texto maior que o botao: alinha a esquerda e deixa o ellipsis cortar.
        x = bounds_.x + padX;
        textW = innerW - glyphW - gap;
        if (textW < 0.f) {
            textW = 0.f;
        }
    }

    if (glyphW > 0.f) {
        const Rect gbox{x, bounds_.y, glyphBox, bounds_.h};
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
    // RESILIENCIA A UAF: o callback pode recriar a lista dona deste botao
    // (ex.: rebuildRows dentro do setOnClick), destruindo o controle no meio
    // da chamada. Callbacks NAO devem destruir o controle sincronamente —
    // mas, por defesa, invalidamos ANTES de invocar e nao tocamos em `this`
    // (nem em membros) depois que o callback roda.
    invalidate();
    if (onClick_) {
        onClick_();
    }
}

Size Button::measure(float constraintWidth)
{
    return {constraintWidth, tokens::kControlHeight * scale_};
}

}  // namespace soundint::ui::controls

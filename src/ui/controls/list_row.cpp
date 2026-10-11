#include "ui/controls/list_row.h"

#include "ui/controls/metrics.h"

namespace soundint::ui::controls {

namespace {
constexpr float kGlyphBox = 20.f;   // caixa do glifo de icone (grid 4px)
}  // namespace

// ---------------------------------------------------------------------------
// Desenho: fundo + overlay de hover/pressionado + glifo + textos. O texto
// para em right - pad*scale - trailingInset (reserva do consumidor).
// ---------------------------------------------------------------------------
void ListRow::render(IRenderTarget& rt, const tokens::Palette& palette)
{
    if (!visible_) {
        return;
    }

    rt.fillRoundedRect(bounds_, metrics::radius(tokens::kRadiusMd, scale_),
                       enabled_ ? palette.surface : palette.surfaceAlt);
    renderOverlay(rt, palette);

    const float padX = tokens::kSpace2 * scale_;
    const float glyphBox = kGlyphBox * scale_;
    const float contentX = bounds_.x + padX;
    const float textX = contentX + glyphBox + padX;
    const float textRight = bounds_.x + bounds_.w - padX - trailingInset_;
    const float textW = textRight - textX;
    const float boxW = textW > 0.f ? textW : 0.f;
    const Color fg = enabled_ ? palette.textPrimary : palette.textSecondary;

    TextStyle icon;
    icon.fontFamily = tokens::kFontIcons;
    icon.fontSize = tokens::kFontBody * scale_;
    icon.weight = tokens::kWeightRegular;
    icon.color = fg;
    icon.align = TextStyle::Align::Center;
    icon.valign = TextStyle::VAlign::Middle;

    if (!glyph_.empty()) {
        const Rect gbox{contentX, bounds_.y, glyphBox, bounds_.h};
        rt.drawGlyph(glyph_, gbox, icon);
    }

    TextStyle title;
    title.fontFamily = tokens::kFontUi;
    title.fontSize = tokens::kFontBody * scale_;
    title.weight = tokens::kWeightSemibold;
    title.color = fg;
    title.align = TextStyle::Align::Left;
    title.valign = TextStyle::VAlign::Middle;

    TextStyle sub;
    sub.fontFamily = tokens::kFontUi;
    sub.fontSize = tokens::kFontCaption * scale_;
    sub.weight = tokens::kWeightRegular;
    sub.color = palette.textSecondary;
    sub.align = TextStyle::Align::Left;
    sub.valign = TextStyle::VAlign::Middle;

    if (subtitle_.empty()) {
        const Rect tbox{textX, bounds_.y, boxW, bounds_.h};
        rt.drawText(title_, tbox, title);
    } else {
        const float halfH = bounds_.h * 0.5f;
        const Rect tbox{textX, bounds_.y, boxW, halfH};
        rt.drawText(title_, tbox, title);
        const Rect sbox{textX, bounds_.y + halfH, boxW, halfH};
        rt.drawText(subtitle_, sbox, sub);
    }

    renderFocus(rt, palette);
}

// ---------------------------------------------------------------------------
// Entrada
// ---------------------------------------------------------------------------
bool ListRow::onPointer(const PointerEvent& e)
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
        // Up de outro botao nao conclui a interacao (segura a captura) —
        // mesmo filtro de button/slider/dropdown/toggle.
        if (e.button != MouseButton::Left) {
            return true;
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

bool ListRow::onKey(const KeyEvent& e)
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

void ListRow::click()
{
    // RESILIENCIA A UAF: o callback pode recriar a lista dona desta linha
    // (rebuild sincrono), destruindo o controle no meio da chamada.
    // Callbacks NAO devem destruir o controle sincronamente — mas, por
    // defesa, invalidamos ANTES e nao tocamos em `this` depois do callback.
    invalidate();
    if (onClick_) {
        onClick_();
    }
}

Size ListRow::measure(float constraintWidth)
{
    return {constraintWidth, tokens::kRowHeight * scale_};
}

}  // namespace soundint::ui::controls

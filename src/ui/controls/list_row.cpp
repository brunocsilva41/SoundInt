#include "ui/controls/list_row.h"

namespace soundint::ui::controls {

namespace {
constexpr float kGlyphBox = 20.f;   // caixa do glifo de icone
}  // namespace

// ---------------------------------------------------------------------------
// Desenho: fundo + overlay de hover/pressionado + glifo + textos
// ---------------------------------------------------------------------------
void ListRow::render(IRenderTarget& rt, const tokens::Palette& palette)
{
    if (!visible_) {
        return;
    }

    rt.fillRoundedRect(bounds_, tokens::kRadiusMd,
                       enabled_ ? palette.surface : palette.surfaceAlt);
    renderOverlay(rt, palette);

    const float contentX = bounds_.x + tokens::kSpace2;
    const float textX = contentX + kGlyphBox + tokens::kSpace2;
    const float textW = (bounds_.x + bounds_.w - tokens::kSpace2) - textX;
    const Color fg = enabled_ ? palette.textPrimary : palette.textSecondary;

    TextStyle icon;
    icon.fontFamily = tokens::kFontIcons;
    icon.fontSize = tokens::kFontBody;
    icon.weight = tokens::kWeightRegular;
    icon.color = fg;
    icon.align = TextStyle::Align::Center;
    icon.valign = TextStyle::VAlign::Middle;

    if (!glyph_.empty()) {
        const Rect gbox{contentX, bounds_.y, kGlyphBox, bounds_.h};
        rt.drawGlyph(glyph_, gbox, icon);
    }

    TextStyle title;
    title.fontFamily = tokens::kFontUi;
    title.fontSize = tokens::kFontBody;
    title.weight = tokens::kWeightSemibold;
    title.color = fg;
    title.align = TextStyle::Align::Left;
    title.valign = TextStyle::VAlign::Middle;

    TextStyle sub;
    sub.fontFamily = tokens::kFontUi;
    sub.fontSize = tokens::kFontCaption;
    sub.weight = tokens::kWeightRegular;
    sub.color = palette.textSecondary;
    sub.align = TextStyle::Align::Left;
    sub.valign = TextStyle::VAlign::Middle;

    if (subtitle_.empty()) {
        const Rect tbox{textX, bounds_.y, textW > 0.f ? textW : 0.f, bounds_.h};
        rt.drawText(title_, tbox, title);
    } else {
        const float halfH = bounds_.h * 0.5f;
        const Rect tbox{textX, bounds_.y, textW > 0.f ? textW : 0.f, halfH};
        rt.drawText(title_, tbox, title);
        const Rect sbox{textX, bounds_.y + halfH, textW > 0.f ? textW : 0.f, halfH};
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
    if (onClick_) {
        onClick_();
    }
    invalidate();
}

Size ListRow::measure(float constraintWidth)
{
    return {constraintWidth, tokens::kRowHeight};
}

}  // namespace soundint::ui::controls

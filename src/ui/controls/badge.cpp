#include "ui/controls/badge.h"

namespace soundint::ui::controls {

Size Badge::naturalSize(IRenderTarget& rt) const
{
    TextStyle ts;
    ts.fontFamily = tokens::kFontUi;
    ts.fontSize = tokens::kFontCaption;
    ts.weight = tokens::kWeightSemibold;
    const Size text = rt.measureText(text_, ts);
    return {text.w + tokens::kSpace2 * 2.f, tokens::kControlHeight};
}

void Badge::render(IRenderTarget& rt, const tokens::Palette& palette)
{
    if (!visible_) {
        return;
    }

    Color fill = palette.surfaceAlt;
    Color fg = palette.textSecondary;
    switch (kind_) {
    case Kind::Accent:
        fill = palette.accent;
        fg = palette.accentFg;
        break;
    case Kind::Success:
        fill = palette.success;
        fg = palette.accentFg;
        break;
    case Kind::Danger:
        fill = palette.danger;
        fg = palette.accentFg;
        break;
    case Kind::Neutral:
    default:
        break;
    }

    const Size nat = naturalSize(rt);
    float w = nat.w;
    if (bounds_.w > 0.f && bounds_.w < w) {
        w = bounds_.w;
    }
    float h = tokens::kControlHeight;
    if (bounds_.h > 0.f && bounds_.h < h) {
        h = bounds_.h;
    }
    const float y = bounds_.h > 0.f ? bounds_.y + (bounds_.h - h) * 0.5f : bounds_.y;
    const Rect cap{bounds_.x, y, w, h};

    rt.fillRoundedRect(cap, tokens::kRadiusSm, fill);

    TextStyle ts;
    ts.fontFamily = tokens::kFontUi;
    ts.fontSize = tokens::kFontCaption;
    ts.weight = tokens::kWeightSemibold;
    ts.color = fg;
    ts.align = TextStyle::Align::Center;
    ts.valign = TextStyle::VAlign::Middle;

    const Rect textBox{cap.x + tokens::kSpace2, cap.y,
                       cap.w - tokens::kSpace2 * 2.f, cap.h};
    rt.drawText(text_, textBox, ts);
}

Size Badge::measure(float constraintWidth)
{
    return {constraintWidth, tokens::kControlHeight};
}

}  // namespace soundint::ui::controls

// ============================================================================
// Base das paginas + campo de texto simples (Track K).
// ============================================================================
#include "ui/windows/settings/settings_page.h"

#include "ui/i18n.h"
#include "ui/renderer.h"

namespace soundint::ui::settings {

// ---------------------------------------------------------------------------
// Ciclo de vida das paginas
// ---------------------------------------------------------------------------
SettingsPage::~SettingsPage() = default;

void SettingsPage::refresh()
{
    retranslateImpl();
    syncImpl();
}

void SettingsPage::layout(const Rect& area, float scale)
{
    area_ = area;
    scale_ = scale;
    layoutImpl(area, scale);        // atualiza contentH_
    const float max = maxScrollY();
    if (scrollY_ > max) {
        scrollY_ = max;
        layoutImpl(area, scale);
    }
}

void SettingsPage::render(IRenderTarget& rt, const tokens::Palette& palette)
{
    if (area_.w <= 0.f || area_.h <= 0.f) {
        return;
    }
    rt.pushClip(area_);
    renderImpl(rt, palette);
    for (Control* control : controls_) {
        if (control->visible()) {
            control->render(rt, palette);
        }
    }
    rt.popClip();
}

// ---------------------------------------------------------------------------
// Entrada
// ---------------------------------------------------------------------------
bool SettingsPage::pointer(const PointerEvent& e, Control*& capture)
{
    if (e.kind == PointerKind::Down && e.button == MouseButton::Left) {
        const bool inside = area_.contains(e.x, e.y);
        for (TextField* field : fields_) {
            field->setFocused(inside && field->bounds().contains(e.x, e.y) &&
                              field->enabled());
        }
        if (!inside) {
            return false;
        }
    }

    for (auto it = controls_.rbegin(); it != controls_.rend(); ++it) {
        Control* control = *it;
        if (control->onPointer(e)) {
            if (e.kind == PointerKind::Down) {
                capture = control;
            }
            return true;
        }
    }

    if (pointerImpl(e)) {
        return true;
    }

    if (e.kind == PointerKind::Wheel && area_.contains(e.x, e.y)) {
        if (maxScrollY() > 0.f) {
            setScrollY(scrollY_ - e.wheelDelta * tokens::kRowHeight * scale_);
        }
        return true;
    }
    return false;
}

bool SettingsPage::key(const KeyEvent& e)
{
    return keyImpl(e);
}

bool SettingsPage::character(wchar_t c)
{
    if (charImpl(c)) {
        return true;
    }
    for (TextField* field : fields_) {
        if (!field->focused() || !field->enabled()) {
            continue;
        }
        if (c == L'\b') {
            field->eraseBack();
            return true;
        }
        if (c >= L' ' && c != 0x7F) {
            field->append(c);
            return true;
        }
        return false;
    }
    return false;
}

bool SettingsPage::rawKey(UINT msg, WPARAM w)
{
    return rawKeyImpl(msg, w);
}

bool SettingsPage::paste(const std::wstring& text)
{
    if (text.empty()) {
        return false;
    }
    for (TextField* field : fields_) {
        if (!field->focused() || !field->enabled()) {
            continue;
        }
        for (const wchar_t c : text) {
            if (c < L' ' || c == 0x7F) {
                continue;
            }
            field->append(c);
        }
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
TextStyle SettingsPage::textStyle(const tokens::Palette& /*palette*/, float fontSize,
                                  int weight, Color color,
                                  TextStyle::Align align) const
{
    TextStyle ts;
    ts.fontFamily = tokens::kFontUi;
    ts.fontSize = fontSize * scale_;
    ts.weight = weight;
    ts.color = color;
    ts.align = align;
    ts.valign = TextStyle::VAlign::Middle;
    return ts;
}

void SettingsPage::drawText(IRenderTarget& rt, const Rect& box, const std::wstring& text,
                            const tokens::Palette& palette, float fontSize, int weight,
                            Color color, TextStyle::Align align) const
{
    rt.drawText(text, box, textStyle(palette, fontSize, weight, color, align));
}

TextField* SettingsPage::addField(TextField* field)
{
    if (field != nullptr) {
        fields_.push_back(field);
        controls_.push_back(field);
    }
    return field;
}

void SettingsPage::setScrollY(float scrollY)
{
    float value = scrollY;
    const float max = maxScrollY();
    if (value < 0.f) {
        value = 0.f;
    }
    if (value > max) {
        value = max;
    }
    if (value == scrollY_) {
        return;
    }
    scrollY_ = value;
    requestRender();
}

float SettingsPage::maxScrollY() const
{
    const float max = contentH_ - area_.h;
    return max > 0.f ? max : 0.f;
}

// ---------------------------------------------------------------------------
// TextField
// ---------------------------------------------------------------------------
void TextField::append(wchar_t c)
{
    if (!editable_ || text_.size() >= kMaxLength) {
        return;
    }
    text_.push_back(c);
    invalidate();
}

void TextField::eraseBack()
{
    if (text_.empty()) {
        return;
    }
    text_.pop_back();
    invalidate();
}

void TextField::render(IRenderTarget& rt, const tokens::Palette& palette)
{
    if (!visible_) {
        return;
    }

    rt.fillRoundedRect(bounds_, tokens::kRadiusMd, palette.surface);
    rt.strokeRoundedRect(bounds_, tokens::kRadiusMd, focused_ ? 1.5f : 1.f,
                         focused_ ? palette.accent : palette.border);
    if (enabled_ && hovered_ && !focused_) {
        renderOverlay(rt, palette);
    }
    if (!enabled_) {
        rt.fillRoundedRect(bounds_, tokens::kRadiusMd, palette.pressed);
    }

    const bool empty = text_.empty();
    TextStyle ts;
    ts.fontFamily = tokens::kFontUi;
    ts.fontSize = tokens::kFontBody * scale_;
    ts.weight = tokens::kWeightRegular;
    ts.color = empty || !enabled_ ? palette.textSecondary : palette.textPrimary;
    ts.align = TextStyle::Align::Left;
    ts.valign = TextStyle::VAlign::Middle;
    const Rect textBox{bounds_.x + tokens::kSpace2 * scale_, bounds_.y,
                       bounds_.w - tokens::kSpace4 * scale_, bounds_.h};
    rt.drawText(empty ? placeholder_ : text_, textBox, ts);
}

bool TextField::onPointer(const PointerEvent& e)
{
    if (!visible_) {
        return false;
    }

    switch (e.kind) {
        case PointerKind::Down:
            if (!enabled_ || e.button != MouseButton::Left ||
                !bounds_.contains(e.x, e.y)) {
                return false;
            }
            captured_ = true;
            pressed_ = true;
            hovered_ = true;
            invalidate();
            return true;

        case PointerKind::Move:
            if (captured_) {
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

        case PointerKind::Leave:
            hovered_ = false;
            pressed_ = false;
            invalidate();
            return false;

        default:
            return false;
    }
}

Size TextField::measure(float constraintWidth)
{
    return {constraintWidth, tokens::kControlHeight * scale_};
}

}  // namespace soundint::ui::settings

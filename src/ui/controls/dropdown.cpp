#include "ui/controls/dropdown.h"

#include "ui/controls/metrics.h"

namespace soundint::ui::controls {

// ---------------------------------------------------------------------------
// Estado
// ---------------------------------------------------------------------------
void Dropdown::setItems(std::vector<std::wstring> items)
{
    items_ = std::move(items);
    if (selected_ != kNoIndex && selected_ >= items_.size()) {
        selected_ = kNoIndex;
    }
    highlight_ = selected_;
    hoverItem_ = kNoIndex;
    invalidate();
}

void Dropdown::setSelectedIndex(size_t index)
{
    selected_ = (index == kNoIndex || index < items_.size()) ? index : kNoIndex;
    highlight_ = selected_;
    invalidate();
}

void Dropdown::setExpanded(bool expanded)
{
    expanded_ = expanded;
    highlight_ = expanded ? selected_ : kNoIndex;
    hoverItem_ = kNoIndex;
    invalidate();
}

void Dropdown::select(size_t index, bool notify)
{
    if (index >= items_.size()) {
        return;
    }
    selected_ = index;
    highlight_ = index;
    // RESILIENCIA A UAF: onChanged_ pode recriar a lista dona deste dropdown
    // (rebuild sincrono). Invalidamos ANTES e nao tocamos em `this` depois
    // que o callback roda.
    invalidate();
    if (notify && onChanged_) {
        onChanged_(index);
    }
}

void Dropdown::stepIndex(size_t& index, int direction)
{
    if (items_.empty()) {
        index = kNoIndex;
        return;
    }
    const ptrdiff_t last = static_cast<ptrdiff_t>(items_.size()) - 1;
    ptrdiff_t cur = (index == kNoIndex)
                        ? ((direction > 0) ? 0 : last)
                        : static_cast<ptrdiff_t>(index) + direction;
    if (cur < 0) {
        cur = 0;
    }
    if (cur > last) {
        cur = last;
    }
    index = static_cast<size_t>(cur);
}

bool Dropdown::stepSelection(int direction)
{
    size_t next = selected_;
    stepIndex(next, direction);
    if (next == selected_) {
        return false;
    }
    select(next, true);
    return true;
}

void Dropdown::collapse()
{
    expanded_ = false;
    hoverItem_ = kNoIndex;
}

// ---------------------------------------------------------------------------
// API aditiva (janelas): estado/retangulo do popup
// ---------------------------------------------------------------------------
bool Dropdown::isPopupOpen() const
{
    return expanded_;
}

Rect Dropdown::popupBounds() const
{
    return expanded_ ? popupRect() : Rect{};
}

// ---------------------------------------------------------------------------
// Geometria pura (escalada por scale_)
// ---------------------------------------------------------------------------
float Dropdown::itemHeight() const
{
    return kItemHeight * scale_;
}

Rect Dropdown::fieldRect() const
{
    const float fieldH = tokens::kControlHeight * scale_;
    const float h = (bounds_.h >= fieldH) ? fieldH : bounds_.h;
    return {bounds_.x, bounds_.y, bounds_.w, h};
}

Rect Dropdown::popupRect() const
{
    const Rect field = fieldRect();
    const float h = static_cast<float>(items_.size()) * itemHeight();
    return {field.x, field.y + field.h, field.w, h};
}

Rect Dropdown::itemRect(size_t index) const
{
    const Rect pop = popupRect();
    const float itemH = itemHeight();
    const float y = pop.y + static_cast<float>(index) * itemH;
    return {pop.x, y, pop.w, itemH};
}

size_t Dropdown::hitItem(float x, float y) const
{
    const Rect pop = popupRect();
    if (items_.empty() || !pop.contains(x, y)) {
        return kNoIndex;
    }
    const size_t i = static_cast<size_t>((y - pop.y) / itemHeight());
    return (i < items_.size()) ? i : kNoIndex;
}

float Dropdown::requiredHeight() const
{
    const Rect pop = popupRect();
    return (pop.y + pop.h) - bounds_.y;
}

// ---------------------------------------------------------------------------
// Desenho: linha fechada + popup flutuante sob pushClip(popupRect)
// ---------------------------------------------------------------------------
void Dropdown::render(IRenderTarget& rt, const tokens::Palette& palette)
{
    if (!visible_) {
        return;
    }

    const float radius = metrics::radius(tokens::kRadiusMd, scale_);
    const float stroke = metrics::stroke(scale_);
    const float padX = tokens::kSpace2 * scale_;
    const float glyphBox = metrics::glyphBox(scale_);

    const Rect field = fieldRect();
    rt.fillRoundedRect(field, radius, enabled_ ? palette.surface : palette.surfaceAlt);
    rt.strokeRoundedRect(field, radius, stroke, palette.border);
    renderOverlay(rt, palette, field);

    const float chevronX = field.x + field.w - padX - glyphBox;
    const float textW = chevronX - padX * 2.f - field.x;
    const std::wstring label =
        (selected_ < items_.size()) ? items_[selected_] : std::wstring{};

    TextStyle ts;
    ts.fontFamily = tokens::kFontUi;
    ts.fontSize = tokens::kFontBody * scale_;
    ts.weight = tokens::kWeightRegular;
    ts.color = enabled_ ? palette.textPrimary : palette.textSecondary;
    ts.align = TextStyle::Align::Left;
    ts.valign = TextStyle::VAlign::Middle;
    const Rect textBox{field.x + padX, field.y, textW > 0.f ? textW : 0.f, field.h};
    rt.drawText(label, textBox, ts);

    TextStyle chevron;
    chevron.fontFamily = tokens::kFontIcons;
    chevron.fontSize = tokens::kFontBody * scale_;
    chevron.weight = tokens::kWeightRegular;
    chevron.color = (enabled_ && expanded_) ? palette.accent : palette.textSecondary;
    chevron.align = TextStyle::Align::Center;
    chevron.valign = TextStyle::VAlign::Middle;
    const Rect chevronBox{chevronX, field.y, glyphBox, field.h};
    rt.drawGlyph(tokens::kIconChevronDown, chevronBox, chevron);

    if (expanded_ && !items_.empty()) {
        const Rect pop = popupRect();
        rt.pushClip(pop);
        rt.fillRoundedRect(pop, radius, palette.surface);
        rt.strokeRoundedRect(pop, radius, stroke, palette.border);

        for (size_t i = 0; i < items_.size(); ++i) {
            const Rect ir = itemRect(i);
            const bool active = enabled_ && (i == hoverItem_ || i == highlight_);
            if (active) {
                rt.fillRoundedRect(ir, radius, palette.hover);
            }

            const bool isSel = (i == selected_);
            TextStyle item;
            item.fontFamily = tokens::kFontUi;
            item.fontSize = tokens::kFontBody * scale_;
            item.weight = isSel ? tokens::kWeightSemibold : tokens::kWeightRegular;
            item.color = isSel ? palette.accent : palette.textPrimary;
            item.align = TextStyle::Align::Left;
            item.valign = TextStyle::VAlign::Middle;

            const float checkW = isSel ? glyphBox + padX : 0.f;
            const float boxW = ir.w - padX * 2.f - checkW;
            const Rect itemBox{ir.x + padX, ir.y, boxW > 0.f ? boxW : 0.f, ir.h};
            rt.drawText(items_[i], itemBox, item);

            if (isSel) {
                const Rect check{ir.x + ir.w - padX - glyphBox, ir.y, glyphBox, ir.h};
                rt.drawGlyph(tokens::kIconCheck, check, item);
            }
        }
        rt.popClip();
    }

    renderFocus(rt, palette, field);
}

// ---------------------------------------------------------------------------
// Entrada
// ---------------------------------------------------------------------------
bool Dropdown::onPointer(const PointerEvent& e)
{
    if (!visible_) {
        return false;
    }

    const Rect field = fieldRect();
    const Rect pop = popupRect();

    switch (e.kind) {
    case PointerKind::Down: {
        if (!enabled_ || e.button != MouseButton::Left) {
            return false;
        }
        const bool inField = field.contains(e.x, e.y);
        const bool inPop = expanded_ && pop.contains(e.x, e.y);
        if (!inField && !inPop) {
            if (expanded_) {
                collapse();
                invalidate();
                return true;
            }
            return false;
        }
        captured_ = true;
        pressed_ = inField;
        hovered_ = inField;
        invalidate();
        return true;
    }

    case PointerKind::Move: {
        if (captured_) {
            // Realce do popup segue o cursor mesmo capturado: o hit por
            // posicao cobre Leave+volta sem congelar o estado visual.
            pressed_ = field.contains(e.x, e.y);
            hoverItem_ = (expanded_ && pop.contains(e.x, e.y)) ? hitItem(e.x, e.y)
                                                              : kNoIndex;
            invalidate();
            return true;
        }
        if (!enabled_) {
            return false;
        }
        const bool inField = field.contains(e.x, e.y);
        size_t hov = kNoIndex;
        if (expanded_ && pop.contains(e.x, e.y)) {
            hov = hitItem(e.x, e.y);
        }
        const bool changed = (hov != hoverItem_) || (inField != hovered_);
        hovered_ = inField;
        hoverItem_ = hov;
        if (changed) {
            invalidate();
        }
        return inField || hov != kNoIndex;
    }

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
        invalidate();
        if (!enabled_) {
            return true;
        }
        if (field.contains(e.x, e.y)) {
            if (expanded_) {
                collapse();
            } else {
                expanded_ = true;
                highlight_ = selected_;
                hoverItem_ = kNoIndex;
            }
            invalidate();
            return true;
        }
        if (expanded_) {
            const size_t idx = hitItem(e.x, e.y);
            if (idx != kNoIndex) {
                select(idx, true);
                collapse();
            } else if (!pop.contains(e.x, e.y)) {
                collapse();
            }
            invalidate();
        }
        return true;
    }

    case PointerKind::Wheel: {
        if (!enabled_) {
            return false;
        }
        const bool inField = field.contains(e.x, e.y);
        const bool inPop = expanded_ && pop.contains(e.x, e.y);
        if ((!inField && !inPop) || items_.empty()) {
            return false;
        }
        const int direction = (e.wheelDelta > 0.f) ? -1 : 1;
        if (expanded_) {
            stepIndex(highlight_, direction);
            invalidate();
        } else {
            stepSelection(direction);
        }
        return true;
    }

    case PointerKind::Leave:
        hovered_ = false;
        hoverItem_ = kNoIndex;
        pressed_ = false;
        invalidate();
        return false;

    default:
        return false;
    }
}

bool Dropdown::onKey(const KeyEvent& e)
{
    if (!visible_ || !enabled_ || !focused_ || !e.down) {
        return false;
    }

    if (expanded_) {
        switch (e.key) {
        case Key::Up:
            stepIndex(highlight_, -1);
            invalidate();
            return true;
        case Key::Down:
            stepIndex(highlight_, 1);
            invalidate();
            return true;
        case Key::Enter:
        case Key::Space:
            if (highlight_ != kNoIndex && highlight_ < items_.size()) {
                select(highlight_, true);
            }
            collapse();
            invalidate();
            return true;
        case Key::Escape:
            collapse();
            invalidate();
            return true;
        default:
            return false;
        }
    }

    switch (e.key) {
    case Key::Up:
    case Key::Down:
        if (items_.empty()) {
            return false;
        }
        stepSelection(e.key == Key::Up ? -1 : 1);
        return true;
    case Key::Enter:
        expanded_ = true;
        highlight_ = selected_;
        hoverItem_ = kNoIndex;
        invalidate();
        return true;
    default:
        return false;
    }
}

Size Dropdown::measure(float constraintWidth)
{
    return {constraintWidth, tokens::kControlHeight * scale_};
}

}  // namespace soundint::ui::controls

#include "ui/controls/list_panel.h"

namespace soundint::ui::controls {

// ---------------------------------------------------------------------------
// Composicao
// ---------------------------------------------------------------------------
void ListPanel::addControl(Control* control)
{
    if (control != nullptr) {
        children_.push_back(control);
    }
}

void ListPanel::clear()
{
    children_.clear();
}

// ---------------------------------------------------------------------------
// Medidas
// ---------------------------------------------------------------------------
float ListPanel::childHeight(Control& child) const
{
    const float h = child.measure(bounds_.w).h;
    return (h > 0.f) ? h : child.bounds().h;
}

float ListPanel::contentHeight() const
{
    float height = 0.f;
    bool first = true;
    for (Control* child : children_) {
        if (!child->visible()) {
            continue;
        }
        if (!first) {
            height += gap_;
        }
        height += childHeight(*child);
        first = false;
    }
    return height;
}

float ListPanel::maxScrollY() const
{
    const float maxScroll = contentHeight() - bounds_.h;
    return maxScroll > 0.f ? maxScroll : 0.f;
}

void ListPanel::setScrollY(float scrollY)
{
    const float maxScroll = maxScrollY();
    float v = scrollY;
    if (v < 0.f) {
        v = 0.f;
    }
    if (v > maxScroll) {
        v = maxScroll;
    }
    if (v == scrollY_) {
        return;
    }
    scrollY_ = v;
    invalidate();
}

// ---------------------------------------------------------------------------
// Layout: filhos em cascata, deslocados por -scrollY
// ---------------------------------------------------------------------------
void ListPanel::layout()
{
    float y = bounds_.y - scrollY_;
    bool first = true;
    for (Control* child : children_) {
        if (!child->visible()) {
            continue;
        }
        if (!first) {
            y += gap_;
        }
        first = false;
        const float h = childHeight(*child);
        child->setBounds({bounds_.x, y, bounds_.w, h});
        y += h;
    }
    // Re-clampa caso os filhos tenham encolhido o conteudo.
    setScrollY(scrollY_);
}

void ListPanel::render(IRenderTarget& rt, const tokens::Palette& palette)
{
    if (!visible_) {
        return;
    }
    layout();
    rt.pushClip(bounds_);
    for (Control* child : children_) {
        if (child->visible()) {
            child->render(rt, palette);
        }
    }
    rt.popClip();
}

// ---------------------------------------------------------------------------
// Entrada: filhos primeiro (topo primeiro); wheel rola o painel
// ---------------------------------------------------------------------------
bool ListPanel::onPointer(const PointerEvent& e)
{
    if (!visible_) {
        return false;
    }
    layout();

    for (auto it = children_.rbegin(); it != children_.rend(); ++it) {
        Control* child = *it;
        if (!child->visible()) {
            continue;
        }
        if (child->onPointer(e)) {
            return true;
        }
    }

    if (e.kind == PointerKind::Wheel && enabled_ && bounds_.contains(e.x, e.y)) {
        setScrollY(scrollY_ - e.wheelDelta * kScrollStep);
        return true;
    }
    return false;
}

bool ListPanel::onKey(const KeyEvent& e)
{
    if (!visible_ || !enabled_) {
        return false;
    }
    for (auto it = children_.rbegin(); it != children_.rend(); ++it) {
        Control* child = *it;
        if (child->visible() && child->onKey(e)) {
            return true;
        }
    }
    return false;
}

Size ListPanel::measure(float constraintWidth)
{
    return {constraintWidth, contentHeight()};
}

}  // namespace soundint::ui::controls

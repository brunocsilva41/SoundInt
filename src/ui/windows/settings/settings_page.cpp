// ============================================================================
// Base das paginas + campo de texto simples (Track K).
// ============================================================================
#include "ui/windows/settings/settings_page.h"

#include "ui/i18n.h"
#include "ui/renderer.h"

#include <algorithm>

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
    applyScale();                // controles existentes escalam antes do layout
    layoutImpl(area, scale);   // atualiza contentH_
    applyScale();                // layoutImpl pode recriar controles (rebuilds)

    const float max = maxScrollY();
    if (scrollY_ > max) {
        scrollY_ = max;
        layoutImpl(area, scale);
    }

    // Popup aberto cortado pela borda inferior da area: rola o minimo
    // necessario para revela-lo e reposiciona (uma re-passada basta).
    if (controls::Dropdown* open = expandedDropdown()) {
        const Rect region = dropdownRegion(*open);
        const float missing = (region.y + region.h) - (area.y + area.h);
        if (missing > 0.f) {
            const float target = std::min(scrollY_ + missing, maxScrollY());
            if (target != scrollY_) {
                scrollY_ = target;
                layoutImpl(area, scale);
                const float clamped = maxScrollY();
                if (scrollY_ > clamped) {
                    scrollY_ = clamped;
                    layoutImpl(area, scale);
                }
            }
        }
    }
}

void SettingsPage::render(IRenderTarget& rt, const tokens::Palette& palette)
{
    if (area_.w <= 0.f || area_.h <= 0.f) {
        return;
    }
    rt.pushClip(area_);
    renderImpl(rt, palette);

    // Popup aberto sempre por cima: desenhado DEPOIS de todos os demais,
    // independentemente da ordem de controls_ — corrige overlay entre
    // dropdowns vizinhos (ex.: Tema coberto pelo Idioma).
    controls::Dropdown* const open = expandedDropdown();
    for (Control* control : controls_) {
        if (control->visible() && control != open) {
            control->render(rt, palette);
        }
    }
    if (open != nullptr) {
        open->render(rt, palette);
    }

    renderScrollbar(rt, palette);
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
            // Clique fora da pagina com popup aberto: apenas fecha o popup.
            if (controls::Dropdown* open = expandedDropdown()) {
                open->setExpanded(false);
                return true;
            }
            focusControl(nullptr);
            return false;
        }
    }

    // Prioridade: dropdown com popup aberto recebe o evento antes dos demais
    // controles (o popup cobre os vizinhos, ex.: campo Idioma sob o Tema).
    // Down fora do popup apenas fecha — nao atravessa para o controle de baixo.
    if (controls::Dropdown* open = expandedDropdown()) {
        if (e.kind == PointerKind::Down && e.button == MouseButton::Left) {
            const Rect region = dropdownRegion(*open);
            if (area_.contains(e.x, e.y) && region.contains(e.x, e.y)) {
                if (open->onPointer(e)) {
                    capture = open;
                    focusControl(open);
                    return true;
                }
            } else {
                open->setExpanded(false);
                return true;
            }
        } else if (e.kind != PointerKind::Down && open->onPointer(e)) {
            return true;   // Move/Wheel/Leave sobre o popup
        }
    }

    for (auto it = controls_.rbegin(); it != controls_.rend(); ++it) {
        Control* control = *it;
        if (control->onPointer(e)) {
            if (e.kind == PointerKind::Down) {
                capture = control;
                // Clique com o mouse da foco de teclado ao controle.
                if (auto* interactive =
                        dynamic_cast<controls::InteractiveControl*>(control)) {
                    focusControl(interactive);
                }
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

    // Clique em area vazia da pagina: limpa o foco de teclado.
    if (e.kind == PointerKind::Down && e.button == MouseButton::Left) {
        focusControl(nullptr);
    }
    return false;
}

CursorShape SettingsPage::cursorAt(float x, float y) const
{
    if (!area_.contains(x, y)) {
        return CursorShape::Arrow;
    }
    // Popup aberto tem prioridade (mesma regra do hit-test de Down).
    if (controls::Dropdown* open = expandedDropdown()) {
        if (dropdownRegion(*open).contains(x, y)) {
            return CursorShape::Hand;
        }
    }
    // Ordem reversa = mesmo criterio de hit-test do pointer().
    for (auto it = controls_.rbegin(); it != controls_.rend(); ++it) {
        const Control* control = *it;
        if (!control->visible() || !control->enabled()) {
            continue;
        }
        if (!control->bounds().contains(x, y)) {
            continue;
        }
        if (const TextField* field = dynamic_cast<const TextField*>(control)) {
            return field->editable() ? CursorShape::Text : CursorShape::Arrow;
        }
        if (dynamic_cast<const controls::InteractiveControl*>(control) !=
            nullptr) {
            return CursorShape::Hand;
        }
        return CursorShape::Arrow;
    }
    return CursorShape::Arrow;
}

bool SettingsPage::key(const KeyEvent& e)
{
    // Controle com foco de teclado primeiro (Enter/Espaco/Setas/Esc de popup).
    if (controls::InteractiveControl* focused = focusedControl()) {
        if (focused->onKey(e)) {
            return true;
        }
    }
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

bool SettingsPage::cycleFocus(bool backward)
{
    // Candidatos na ordem de layout: visiveis e habilitados.
    std::vector<controls::InteractiveControl*> candidates;
    for (Control* control : controls_) {
        auto* interactive = dynamic_cast<controls::InteractiveControl*>(control);
        if (interactive != nullptr && interactive->visible() && interactive->enabled()) {
            candidates.push_back(interactive);
        }
    }
    if (candidates.empty()) {
        return false;
    }

    // Posicao atual pela propria flag de foco (cobre foco vindo do mouse).
    ptrdiff_t current = -1;
    for (size_t i = 0; i < candidates.size(); ++i) {
        if (candidates[i]->focused()) {
            current = static_cast<ptrdiff_t>(i);
            break;
        }
    }

    const ptrdiff_t count = static_cast<ptrdiff_t>(candidates.size());
    ptrdiff_t next;
    if (current < 0) {
        next = backward ? count - 1 : 0;
    } else {
        next = current + (backward ? -1 : 1);
        if (next < 0) {
            next = count - 1;
        } else if (next >= count) {
            next = 0;
        }
    }

    // Navegacao por teclado fecha popups: um controle focado por vez.
    if (controls::Dropdown* open = expandedDropdown()) {
        open->setExpanded(false);
    }
    focusControl(candidates[static_cast<size_t>(next)]);
    return true;
}

void SettingsPage::onHide()
{
    if (controls::Dropdown* open = expandedDropdown()) {
        open->setExpanded(false);
    }
    focusControl(nullptr);
}

// ---------------------------------------------------------------------------
// Infra privada
// ---------------------------------------------------------------------------
void SettingsPage::applyScale()
{
    for (Control* control : controls_) {
        control->setScale(scale_);
    }
}

controls::Dropdown* SettingsPage::expandedDropdown() const
{
    // De tras para frente: se houver dois abertos (nao deveria), o desenhado
    // por cima (ultimo na ordem de controls_) e o vencedor.
    for (auto it = controls_.rbegin(); it != controls_.rend(); ++it) {
        auto* dropdown = dynamic_cast<controls::Dropdown*>(*it);
        if (dropdown != nullptr && dropdown->visible() && dropdown->expanded()) {
            return dropdown;
        }
    }
    return nullptr;
}

Rect SettingsPage::dropdownRegion(const controls::Dropdown& dropdown) const
{
    Rect region = dropdown.bounds();
    const Rect popup = dropdown.popupRect();
    const float bottom = std::max(region.y + region.h, popup.y + popup.h);
    return {region.x, region.y, region.w, bottom - region.y};
}

void SettingsPage::renderScrollbar(IRenderTarget& rt, const tokens::Palette& palette) const
{
    const float max = maxScrollY();
    if (max <= 0.f || contentH_ <= 0.f) {
        return;
    }

    // Trilha fina na borda direita da pagina + thumb proporcional ao trecho
    // visivel do conteudo (scroll so existe via wheel; o visual e indicador).
    const float trackW = 4.f * scale_;
    const Rect track{area_.x + area_.w - trackW, area_.y, trackW, area_.h};
    rt.fillRoundedRect(track, trackW * 0.5f, palette.border);

    float thumbH = area_.h * (area_.h / contentH_);
    const float minThumb = 24.f * scale_;
    if (thumbH < minThumb) {
        thumbH = minThumb;
    }
    if (thumbH > track.h) {
        thumbH = track.h;
    }
    const float travel = track.h - thumbH;
    const float position = travel * (scrollY_ / max);
    rt.fillRoundedRect({track.x, track.y + position, trackW, thumbH},
                       trackW * 0.5f, palette.textSecondary);
}

void SettingsPage::focusControl(controls::InteractiveControl* target)
{
    bool changed = false;
    for (Control* control : controls_) {
        auto* interactive = dynamic_cast<controls::InteractiveControl*>(control);
        if (interactive == nullptr) {
            continue;
        }
        if (interactive == target) {
            if (!interactive->focused()) {
                interactive->setFocused(true);
                changed = true;
            }
        } else if (interactive->focused()) {
            interactive->setFocused(false);
            changed = true;
        }
    }
    focused_ = target;
    if (changed) {
        requestRender();
    }
}

controls::InteractiveControl* SettingsPage::focusedControl()
{
    // focused_ pode apontar para controle destruido por um rebuild da pagina
    // (ex.: rules rebuildRows): valida pertinencia a controls_ antes de usar.
    for (Control* control : controls_) {
        if (control != focused_) {
            continue;
        }
        auto* interactive = dynamic_cast<controls::InteractiveControl*>(control);
        if (interactive != nullptr && interactive->visible() && interactive->enabled()) {
            return interactive;
        }
        break;
    }
    focused_ = nullptr;
    return nullptr;
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
            // Up de outro botao nao conclui a interacao (segura a captura).
            if (e.button != MouseButton::Left) {
                return true;
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

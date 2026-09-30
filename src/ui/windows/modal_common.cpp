// ============================================================================
// Modais popup (Track I) — fila/manager, posicionamento puro, controles
// internos e roteamento de entrada das janelas modal.
// ============================================================================
#include "ui/windows/modal_common.h"

#include "ui/windows/modal_app.h"
#include "ui/windows/modal_device.h"

#include <algorithm>
#include <cstddef>
#include <cwctype>
#include <memory>

namespace soundint::ui::modal {

// ---------------------------------------------------------------------------
// Posicionamento puro
// ---------------------------------------------------------------------------
Rect computePopupRect(POINT anchor, Size size, const RECT& workArea)
{
    const float w = (std::max)(0.f, size.w);
    const float h = (std::max)(0.f, size.h);

    const float areaW = static_cast<float>(workArea.right - workArea.left);
    const float areaH = static_cast<float>(workArea.bottom - workArea.top);
    const bool degenerate = (areaW <= 0.f) || (areaH <= 0.f);
    if (degenerate) {
        // Sem area de trabalho utilizavel: ancora no proprio ponto.
        return {static_cast<float>(anchor.x) - w, static_cast<float>(anchor.y) - h, w, h};
    }

    // Reduz para caber 100% contido (com a margem das duas bordas).
    const float availW = (std::max)(0.f, areaW - kPopupMargin * 2.f);
    const float availH = (std::max)(0.f, areaH - kPopupMargin * 2.f);
    const float drawW = (std::min)(w, availW);
    const float drawH = (std::min)(h, availH);

    // Canto inferior direito da area de trabalho (monitor do anchor).
    float x = static_cast<float>(workArea.right) - kPopupMargin - drawW;
    float y = static_cast<float>(workArea.bottom) - kPopupMargin - drawH;

    const float minX = static_cast<float>(workArea.left) + kPopupMargin;
    const float minY = static_cast<float>(workArea.top) + kPopupMargin;
    x = (std::max)(minX, x);
    y = (std::max)(minY, y);
    return {x, y, drawW, drawH};
}

// ---------------------------------------------------------------------------
// Fila FIFO pura
// ---------------------------------------------------------------------------
PendingModal PendingModal::forDevice(const DeviceInfo& device)
{
    PendingModal item;
    item.kind = Kind::Device;
    item.device = device;
    return item;
}

PendingModal PendingModal::forApp(const SessionInfo& session)
{
    PendingModal item;
    item.kind = Kind::App;
    item.app = session;
    return item;
}

bool ModalQueue::push(PendingModal item)
{
    bool dropped = false;
    if (items_.size() >= kQueueCapacity) {
        items_.pop_front();   // estourou: descarta o mais antigo
        dropped = true;
    }
    items_.push_back(std::move(item));
    return dropped;
}

const PendingModal& ModalQueue::front() const
{
    return items_.front();
}

void ModalQueue::pop()
{
    if (!items_.empty()) {
        items_.pop_front();
    }
}

void ModalQueue::clear()
{
    items_.clear();
}

// ---------------------------------------------------------------------------
// Utilidades puras
// ---------------------------------------------------------------------------
std::wstring trimCollapse(std::wstring text)
{
    std::wstring out;
    out.reserve(text.size());
    bool pendingSpace = false;
    bool started = false;
    for (const wchar_t ch : text) {
        if (std::iswspace(ch)) {
            if (started) {
                pendingSpace = true;
            }
            continue;
        }
        if (pendingSpace) {
            out.push_back(L' ');
            pendingSpace = false;
        }
        out.push_back(ch);
        started = true;
    }
    return out;
}

std::wstring arrivalPatternFrom(const DeviceInfo& device)
{
    return trimCollapse(device.friendlyName);
}

std::wstring normalizeProcessName(std::wstring processName)
{
    processName = trimCollapse(std::move(processName));
    for (wchar_t& ch : processName) {
        ch = static_cast<wchar_t>(std::towlower(ch));
    }
    return processName;
}

size_t initialOutputIndex(const std::vector<DeviceInfo>& outputs,
                          const std::wstring& currentDeviceId)
{
    if (currentDeviceId.empty()) {
        return 0;   // segue o default do sistema
    }
    for (size_t i = 0; i < outputs.size(); ++i) {
        if (outputs[i].id == currentDeviceId) {
            return i + 1;   // indice 0 e reservado para "Padrao do sistema"
        }
    }
    return 0;   // id desconhecido (device saiu?) -> sistema
}

std::wstring deviceGlyphFor(const std::wstring& friendlyName)
{
    std::wstring lower = friendlyName;
    for (wchar_t& ch : lower) {
        ch = static_cast<wchar_t>(std::towlower(ch));
    }
    const bool headphones =
        (lower.find(L"head") != std::wstring::npos) ||
        (lower.find(L"fone") != std::wstring::npos) ||
        (lower.find(L"auricul") != std::wstring::npos) ||
        (lower.find(L"airpod") != std::wstring::npos);
    return headphones ? tokens::kIconHeadphones : tokens::kIconSpeaker;
}

// ---------------------------------------------------------------------------
// CheckBox — caixa 16px + kIconCheck + rotulo; Enter fica livre para a
// acao primaria da janela (somente Space alterna).
// ---------------------------------------------------------------------------
Rect CheckBox::boxRect() const
{
    return {bounds_.x, bounds_.y + (bounds_.h - kBoxSize) * 0.5f, kBoxSize, kBoxSize};
}

void CheckBox::render(IRenderTarget& rt, const tokens::Palette& palette)
{
    if (!visible_) {
        return;
    }

    renderOverlay(rt, palette);

    const Rect box = boxRect();
    rt.fillRoundedRect(box, tokens::kRadiusSm, value_ ? palette.accent : palette.surface);
    rt.strokeRoundedRect(box, tokens::kRadiusSm, 1.f,
                         value_ ? palette.accent : palette.border);

    if (value_) {
        TextStyle glyph;
        glyph.fontFamily = tokens::kFontIcons;
        glyph.fontSize = tokens::kFontCaption;
        glyph.weight = tokens::kWeightRegular;
        glyph.color = palette.accentFg;
        glyph.align = TextStyle::Align::Center;
        glyph.valign = TextStyle::VAlign::Middle;
        rt.drawGlyph(tokens::kIconCheck, box, glyph);
    }

    TextStyle text;
    text.fontFamily = tokens::kFontUi;
    text.fontSize = tokens::kFontBody;
    text.weight = tokens::kWeightRegular;
    text.color = enabled_ ? palette.textPrimary : palette.textSecondary;
    text.align = TextStyle::Align::Left;
    text.valign = TextStyle::VAlign::Middle;
    const float textX = box.x + kBoxSize + tokens::kSpace2;
    const Rect textBox{textX, bounds_.y,
                       (std::max)(0.f, bounds_.x + bounds_.w - textX), bounds_.h};
    rt.drawText(text_, textBox, text);

    renderFocus(rt, palette);
}

bool CheckBox::onPointer(const PointerEvent& e)
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
                toggle();
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

bool CheckBox::onKey(const KeyEvent& e)
{
    if (!visible_ || !enabled_ || !focused_ || !e.down) {
        return false;
    }
    if (e.key == Key::Space) {
        toggle();
        return true;
    }
    return false;
}

void CheckBox::toggle()
{
    value_ = !value_;
    invalidate();
    if (onChanged_) {
        onChanged_(value_);
    }
}

// ---------------------------------------------------------------------------
// AccentButton — estilo primario (accent/accentFg), mesma ergonomia do
// botao do Track E.
// ---------------------------------------------------------------------------
void AccentButton::render(IRenderTarget& rt, const tokens::Palette& palette)
{
    if (!visible_) {
        return;
    }

    rt.fillRoundedRect(bounds_, tokens::kRadiusMd,
                       enabled_ ? palette.accent : palette.surfaceAlt);
    renderOverlay(rt, palette);

    TextStyle ts;
    ts.fontFamily = tokens::kFontUi;
    ts.fontSize = tokens::kFontBody;
    ts.weight = tokens::kWeightRegular;
    ts.color = enabled_ ? palette.accentFg : palette.textSecondary;
    ts.align = TextStyle::Align::Center;
    ts.valign = TextStyle::VAlign::Middle;
    rt.drawText(text_, bounds_, ts);

    renderFocus(rt, palette);
}

bool AccentButton::onPointer(const PointerEvent& e)
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

bool AccentButton::onKey(const KeyEvent& e)
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

void AccentButton::click()
{
    if (onClick_) {
        onClick_();
    }
    invalidate();
}

// ---------------------------------------------------------------------------
// ModalWindow — frame, roteamento de pointer/foco/teclado
// ---------------------------------------------------------------------------
void ModalWindow::addControl(controls::InteractiveControl& control, bool defaultFocus)
{
    controls_.push_back(&control);
    if (defaultFocus) {
        defaultFocus_ = &control;
    }
}

void ModalWindow::resetInteraction()
{
    captured_ = nullptr;
    hovered_ = nullptr;
    for (controls::InteractiveControl* control : controls_) {
        control->setFocused(false);
    }
    focused_ = defaultFocus_;
    if (focused_) {
        focused_->setFocused(true);
    }
}

bool ModalWindow::present(POINT anchor)
{
    anchor_ = anchor;
    if (!valid()) {
        if (!create(GetModuleHandleW(nullptr), windowClassName(),
                    WS_EX_TOPMOST | WS_EX_TOOLWINDOW)) {
            return false;
        }
    }
    resetInteraction();
    reframe();
    return isVisible();
}

void ModalWindow::reframe()
{
    if (!valid()) {
        return;
    }

    RECT workArea{};
    if (!workAreaAt(anchor_, workArea)) {
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
    }

    const float scale = dpiScale();
    const Size logical = logicalContentSize();
    const Size pixels{logical.w * scale, logical.h * scale};
    const Rect rect = computePopupRect(anchor_, pixels, workArea);

    resizeClient(static_cast<int>(rect.w + 0.5f), static_cast<int>(rect.h + 0.5f));
    const bool firstShow = !isVisible();
    showAt(static_cast<int>(rect.x + 0.5f), static_cast<int>(rect.y + 0.5f), firstShow);
}

controls::InteractiveControl* ModalWindow::hitTest(float x, float y) const
{
    for (auto it = controls_.rbegin(); it != controls_.rend(); ++it) {
        controls::InteractiveControl* control = *it;
        if (control->visible() && control->enabled() && control->bounds().contains(x, y)) {
            return control;
        }
    }
    return nullptr;
}

bool ModalWindow::focusable(const controls::InteractiveControl* control) const
{
    return control && control->visible() && control->enabled();
}

void ModalWindow::setFocus(controls::InteractiveControl* control)
{
    if (focused_ == control) {
        return;
    }
    for (controls::InteractiveControl* item : controls_) {
        item->setFocused(item == control);
    }
    focused_ = control;
}

void ModalWindow::cycleFocus(int direction)
{
    if (controls_.empty()) {
        return;
    }
    ptrdiff_t index = -1;
    for (size_t i = 0; i < controls_.size(); ++i) {
        if (controls_[i] == focused_) {
            index = static_cast<ptrdiff_t>(i);
            break;
        }
    }
    const ptrdiff_t count = static_cast<ptrdiff_t>(controls_.size());
    for (ptrdiff_t step = 1; step <= count; ++step) {
        ptrdiff_t next = index + static_cast<ptrdiff_t>(direction) * step;
        next = ((next % count) + count) % count;
        if (focusable(controls_[static_cast<size_t>(next)])) {
            setFocus(controls_[static_cast<size_t>(next)]);
            return;
        }
    }
}

bool ModalWindow::onPointer(const PointerEvent& e)
{
    switch (e.kind) {
        case PointerKind::Down: {
            if (captured_) {
                captured_->onPointer(e);
                syncState();
                return true;
            }
            controls::InteractiveControl* target = hitTest(e.x, e.y);
            if (preparePointerDown(target)) {
                syncState();
                return true;
            }
            if (!target) {
                syncState();
                return false;
            }
            setFocus(target);
            const bool consumed = target->onPointer(e);
            if (consumed) {
                captured_ = target;
            }
            syncState();
            return consumed;
        }

        case PointerKind::Move: {
            if (captured_) {
                const bool consumed = captured_->onPointer(e);
                syncState();
                return consumed;
            }
            controls::InteractiveControl* target = hitTest(e.x, e.y);
            if (hovered_ && hovered_ != target) {
                hovered_->onPointer(e);   // limpa o hover antigo
            }
            hovered_ = target;
            if (!target) {
                syncState();
                return false;
            }
            const bool consumed = target->onPointer(e);
            syncState();
            return consumed;
        }

        case PointerKind::Up: {
            if (captured_) {
                controls::InteractiveControl* target = captured_;
                captured_ = nullptr;
                const bool consumed = target->onPointer(e);
                syncState();
                return consumed;
            }
            controls::InteractiveControl* target = hitTest(e.x, e.y);
            if (target) {
                const bool consumed = target->onPointer(e);
                syncState();
                return consumed;
            }
            syncState();
            return false;
        }

        case PointerKind::Wheel: {
            controls::InteractiveControl* target =
                captured_ ? captured_ : hitTest(e.x, e.y);
            if (!target) {
                syncState();
                return false;
            }
            const bool consumed = target->onPointer(e);
            syncState();
            return consumed;
        }

        case PointerKind::Leave: {
            if (captured_) {
                captured_->onPointer(e);
                syncState();
                return true;
            }
            if (hovered_) {
                hovered_->onPointer(e);
                hovered_ = nullptr;
            }
            syncState();
            return false;
        }

        default:
            syncState();
            return false;
    }
}

bool ModalWindow::onKey(const KeyEvent& e)
{
    // Ordem do contrato: controle focado -> foco padrao -> janela.
    if (focused_ && focused_->onKey(e)) {
        syncState();
        return true;
    }
    if (defaultFocus_ && defaultFocus_ != focused_ && defaultFocus_->onKey(e)) {
        syncState();
        return true;
    }
    if (e.down) {
        switch (e.key) {
            case Key::Escape:
                onCancelAction();
                syncState();
                return true;
            case Key::Enter:
                onPrimaryAction();
                syncState();
                return true;
            case Key::Tab:
                cycleFocus(e.shift ? -1 : 1);
                syncState();
                return true;
            default:
                break;
        }
    }
    syncState();
    return false;
}

void ModalWindow::onRender(IRenderTarget& rt, const Rect& client)
{
    (void)client;
    const tokens::Palette pal = palette();
    const float scale = dpiScale();
    layoutControls(scale);
    rt.clear(pal.windowBg);
    paint(rt, scale, pal);
    for (controls::InteractiveControl* control : controls_) {
        if (control->visible()) {
            control->render(rt, pal);
        }
    }
}

void ModalWindow::onDestroy()
{
    captured_ = nullptr;
    hovered_ = nullptr;
    focused_ = nullptr;
    if (onClosed_) {
        onClosed_(this);
    }
}

// ---------------------------------------------------------------------------
// Manager: seam, fila e exibicao (1 modal por vez)
// ---------------------------------------------------------------------------
namespace {

struct Manager {
    ModalHost host;
    bool initialized = false;
    bool showing = false;
    bool timerPending = false;
    ModalQueue queue;
    std::unique_ptr<ModalDeviceWindow> deviceWindow;
    std::unique_ptr<ModalAppWindow> appWindow;
};

constexpr UINT_PTR kShowNextTimerId = 0x534D49;   // "SMI"

Manager& manager()
{
    static Manager instance;
    return instance;
}

void showNext();

VOID CALLBACK showNextTimer(HWND, UINT, UINT_PTR id, DWORD)
{
    KillTimer(nullptr, id);
    Manager& m = manager();
    m.timerPending = false;
    showNext();
}

// O proximo modal so e criado apos o DestroyWindow atual terminar (foco/
// foreground ficariam instaveis dentro do WM_DESTROY): adia via timer de 1ms.
void scheduleShowNext()
{
    Manager& m = manager();
    if (m.timerPending || !m.initialized || m.queue.empty()) {
        return;
    }
    if (SetTimer(nullptr, kShowNextTimerId, 1, &showNextTimer) == 0) {
        m.timerPending = false;
        return;
    }
    m.timerPending = true;
}

void onModalClosed(ModalWindow*)
{
    Manager& m = manager();
    m.showing = false;
    scheduleShowNext();
}

void showNext()
{
    Manager& m = manager();
    if (!m.initialized || m.showing || m.queue.empty()) {
        return;
    }

    POINT anchor{};
    if (!GetCursorPos(&anchor)) {
        anchor = POINT{0, 0};   // fallback: monitor primario
    }

    // Falha de criacao descarta o pedido e tenta o proximo (teto: capacidade).
    for (size_t attempts = 0; attempts <= kQueueCapacity && !m.showing &&
                              !m.queue.empty();
         ++attempts) {
        PendingModal item = m.queue.front();
        m.queue.pop();

        ModalWindow* window = nullptr;
        if (item.kind == PendingModal::Kind::Device) {
            if (!m.deviceWindow) {
                m.deviceWindow = std::make_unique<ModalDeviceWindow>();
                m.deviceWindow->setOnClosed(&onModalClosed);
            }
            m.deviceWindow->setDevice(item.device);
            window = m.deviceWindow.get();
        } else {
            if (!m.appWindow) {
                m.appWindow = std::make_unique<ModalAppWindow>();
                m.appWindow->setOnClosed(&onModalClosed);
            }
            m.appWindow->setSession(item.app);
            window = m.appWindow.get();
        }

        window->setHost(&m.host);
        if (window->present(anchor)) {
            m.showing = true;
        }
    }
}

}  // namespace

ModalQueue& pendingQueue()
{
    return manager().queue;
}

void init(ModalHost host)
{
    Manager& m = manager();
    m.host = std::move(host);
    m.initialized = true;
}

void requestNewDevice(const DeviceInfo& device)
{
    Manager& m = manager();
    if (!m.initialized) {
        return;   // antes do boot: no-op
    }
    m.queue.push(PendingModal::forDevice(device));
    showNext();
}

void requestNewApp(const SessionInfo& session)
{
    Manager& m = manager();
    if (!m.initialized) {
        return;   // antes do boot: no-op
    }
    m.queue.push(PendingModal::forApp(session));
    showNext();
}

void shutdown()
{
    Manager& m = manager();
    if (m.timerPending) {
        KillTimer(nullptr, kShowNextTimerId);
        m.timerPending = false;
    }
    m.queue.clear();          // limpa antes de destruir (evita reexibicao)
    m.initialized = false;
    m.host = ModalHost{};
    m.deviceWindow.reset();   // dtor -> DestroyWindow -> onModalClosed (no-op)
    m.appWindow.reset();
    m.showing = false;
}

bool isVisible()
{
    Manager& m = manager();
    if (!m.showing) {
        return false;
    }
    const bool deviceVisible = m.deviceWindow && m.deviceWindow->isVisible();
    const bool appVisible = m.appWindow && m.appWindow->isVisible();
    return deviceVisible || appVisible;
}

}  // namespace soundint::ui::modal

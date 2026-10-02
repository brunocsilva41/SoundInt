// ============================================================================
// Janela de Configuracoes (Track K): chrome customizado (barra de titulo +
// navegacao lateral), paginas, persistencia via Store e seam de integracao.
// ============================================================================
#include "ui/windows/settings/settings_window.h"

#include "ui/windows/settings/settings_page.h"

#include "core/store.h"
#include "ui/i18n.h"
#include "ui/renderer.h"
#include "ui/windows/window_base.h"

#include <windows.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace soundint::ui::settings {
namespace {

// Dimensoes logicas (96 DPI) — multiplicadas por dpiScale().
constexpr float kTitleBarH = 40.f;
constexpr float kNavW = 200.f;
constexpr float kWindowW = 760.f;
constexpr float kWindowH = 540.f;
constexpr float kNavItemH = 36.f;
constexpr float kNavGap = 4.f;
constexpr float kCloseSize = 32.f;
constexpr float kPad = 24.f;
constexpr float kHeadingH = 24.f;

constexpr wchar_t kClassName[] = L"SoundIntSettingsWindow";
constexpr wchar_t kRunKey[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t kRunValueName[] = L"SoundInt";

enum PageIndex : size_t {
    kGeneral = 0,
    kRules,
    kProfiles,
    kHotkeys,
    kUpdates,
    kAbout,
    kPageCount
};

struct NavSpec {
    const wchar_t* key;
    const wchar_t* glyph;
};

constexpr NavSpec kNavSpecs[kPageCount] = {
    {L"settings.nav.general", tokens::kIconSettings},
    {L"settings.nav.rules", tokens::kIconSpeaker},
    {L"settings.nav.profiles", tokens::kIconHeadphones},
    {L"settings.nav.hotkeys", tokens::kIconKeyboard},
    {L"settings.nav.updates", tokens::kIconDownload},
    {L"settings.nav.about", tokens::kIconInfo},
};

// Host injetado pelo integrador (endereco estavel; init() troca o conteudo).
SettingsHost& hostRef()
{
    static SettingsHost host;
    return host;
}

// Resultados do checkForUpdates: o callback pode chegar de outra thread.
struct UpdateResult {
    bool found = false;
    std::wstring info;
};

std::mutex g_resultMutex;
std::vector<UpdateResult> g_results;
std::atomic<HWND> g_hwnd{nullptr};

// ---------------------------------------------------------------------------
// Auxiliares puros do registro de inicializacao
// ---------------------------------------------------------------------------
std::wstring quoted(const std::wstring& value)
{
    return L"\"" + value + L"\"";
}

// ---------------------------------------------------------------------------
// Janela
// ---------------------------------------------------------------------------
class SettingsWindow : public WindowBase {
public:
    SettingsWindow() = default;
    SettingsWindow(const SettingsWindow&) = delete;
    SettingsWindow& operator=(const SettingsWindow&) = delete;

    bool ensureCreated();
    void showCentered();
    void refreshAll();
    // Recarrega textos de todas as paginas e re-posiciona (troca de idioma).
    void refreshAndLayout()
    {
        refreshAll();
        layout();
        requestRender();
    }

protected:
    bool handleMessage(UINT msg, WPARAM w, LPARAM l, LRESULT& result) override;
    void onRender(IRenderTarget& rt, const Rect& client) override;
    void onDestroy() override;
    bool onPointer(const PointerEvent& e) override;
    bool onKey(const KeyEvent& e) override;
    bool onChar(wchar_t c) override;

private:
    void createPages();
    void layout();
    void selectPage(size_t index);
    void drainResults();
    bool pasteToField();
    SettingsPage* page() const;
    size_t navHit(float x, float y) const;
    float s(float v) const { return v * scale_; }

    std::unique_ptr<SettingsPage> pages_[kPageCount];
    size_t selected_ = kGeneral;
    size_t navHover_ = kPageCount;   // kPageCount = nenhum
    controls::Button closeButton_;
    Control* capture_ = nullptr;

    float scale_ = 1.f;
    Rect titleBar_{};
    Rect navRect_{};
    Rect contentRect_{};
    Rect headingRect_{};
    Rect pageArea_{};
    Rect navItems_[kPageCount];
};

SettingsWindow& instance()
{
    static SettingsWindow window;
    return window;
}

SettingsPage* SettingsWindow::page() const
{
    return pages_[selected_].get();
}

void SettingsWindow::createPages()
{
    if (pages_[kGeneral]) {
        return;
    }
    pages_[kGeneral] = createGeneralPage();
    pages_[kRules] = createRulesPage();
    pages_[kProfiles] = createProfilesPage();
    pages_[kHotkeys] = createHotkeysPage();
    pages_[kUpdates] = createUpdatesPage();
    pages_[kAbout] = createAboutPage();
    for (auto& pagePtr : pages_) {
        if (pagePtr) {
            pagePtr->refresh();
        }
    }
}

bool SettingsWindow::ensureCreated()
{
    if (valid()) {
        return true;
    }
    createPages();
    if (!create(GetModuleHandleW(nullptr), kClassName, WS_EX_TOOLWINDOW, WS_POPUP)) {
        return false;
    }
    g_hwnd.store(hwnd());
    closeButton_.setGlyph(tokens::kIconClose);
    closeButton_.setAccessibleName(tr(L"common.close"));
    closeButton_.setOnClick([this]() { hide(); });
    layout();
    return true;
}

void SettingsWindow::showCentered()
{
    drainResults();
    refreshAll();

    const float scale = dpiScale();
    const int width = static_cast<int>(kWindowW * scale);
    const int height = static_cast<int>(kWindowH * scale);
    resizeClient(width, height);

    POINT pt{};
    GetCursorPos(&pt);
    RECT work{};
    int x = 0;
    int y = 0;
    if (workAreaAt(pt, work)) {
        x = work.left + ((work.right - work.left) - width) / 2;
        y = work.top + ((work.bottom - work.top) - height) / 2;
    }

    layout();
    showAt(x, y, true);
}

void SettingsWindow::refreshAll()
{
    // capture_ guarda um ponteiro cru para controles que as paginas vao
    // destruir em rebuildRows(); soltar antes evita UAF no proximo WM_MOUSE*
    // (cenario: 2a instancia -> show -> refreshAll durante botao pressionado).
    if (capture_ != nullptr) {
        capture_ = nullptr;
        releaseMouse();
    }
    for (auto& pagePtr : pages_) {
        if (pagePtr) {
            pagePtr->refresh();
        }
    }
}

void SettingsWindow::selectPage(size_t index)
{
    if (index >= kPageCount || index == selected_) {
        return;
    }
    if (capture_ != nullptr) {
        capture_ = nullptr;
        releaseMouse();
    }
    SettingsPage* oldPage = page();
    if (oldPage != nullptr) {
        oldPage->onHide();
    }
    selected_ = index;
    if (pages_[selected_]) {
        pages_[selected_]->refresh();
    }
    layout();
    requestRender();
}

void SettingsWindow::drainResults()
{
    bool found = false;
    std::wstring info;
    bool any = false;
    while (takeUpdateResult(found, info)) {
        any = true;
        for (auto& pagePtr : pages_) {
            if (pagePtr) {
                pagePtr->updateResult(found, info);
            }
        }
    }
    if (any) {
        requestRender();
    }
}

// ---------------------------------------------------------------------------
// Geometria
// ---------------------------------------------------------------------------
void SettingsWindow::layout()
{
    const Rect client = clientBounds();
    scale_ = dpiScale();

    const float titleH = s(kTitleBarH);
    const float navW = s(kNavW);
    titleBar_ = {0.f, 0.f, client.w, titleH};

    const float closeSize = s(kCloseSize);
    closeButton_.setBounds({client.w - s(8) - closeSize, (titleH - closeSize) * 0.5f,
                            closeSize, closeSize});

    navRect_ = {0.f, titleH, navW, client.h - titleH};
    const float itemTop = titleH + s(12);
    const float itemH = s(kNavItemH);
    for (size_t i = 0; i < kPageCount; ++i) {
        navItems_[i] = {s(8), itemTop + static_cast<float>(i) * (itemH + s(kNavGap)),
                        navW - s(16), itemH};
    }

    contentRect_ = {navW, titleH, client.w - navW, client.h - titleH};
    headingRect_ = {contentRect_.x + s(kPad), contentRect_.y + s(16),
                    contentRect_.w - s(kPad) * 2.f, s(kHeadingH)};
    const float pageY = headingRect_.y + headingRect_.h + s(8);
    pageArea_ = {contentRect_.x + s(kPad), pageY, contentRect_.w - s(kPad) * 2.f,
                 contentRect_.y + contentRect_.h - pageY - s(16)};

    SettingsPage* activePage = page();
    if (activePage != nullptr) {
        activePage->layout(pageArea_, scale_);
    }
}

size_t SettingsWindow::navHit(float x, float y) const
{
    for (size_t i = 0; i < kPageCount; ++i) {
        if (navItems_[i].contains(x, y)) {
            return i;
        }
    }
    return kPageCount;
}

// ---------------------------------------------------------------------------
// Mensagens
// ---------------------------------------------------------------------------
bool SettingsWindow::handleMessage(UINT msg, WPARAM w, LPARAM l, LRESULT& result)
{
    switch (msg) {
        case WM_NCHITTEST: {
            POINT pt{static_cast<short>(LOWORD(l)), static_cast<short>(HIWORD(l))};
            ScreenToClient(hwnd(), &pt);
            layout();
            const bool inTitle = pt.y >= 0 && pt.y < static_cast<LONG>(titleBar_.h);
            const bool inClose = closeButton_.bounds().contains(
                static_cast<float>(pt.x), static_cast<float>(pt.y));
            if (inTitle && !inClose) {
                result = HTCAPTION;   // arraste da barra de titulo
                return true;
            }
            return false;
        }
        case WM_CLOSE:
            hide();                    // fecha sem destruir (janela fica viva)
            result = 0;
            return true;
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN: {
            SettingsPage* activePage = page();
            if (activePage != nullptr && activePage->rawKey(msg, w)) {
                result = 0;
                return true;
            }
            const bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
            if (ctrl && (w == L'V' || w == L'v') && pasteToField()) {
                result = 0;
                return true;
            }
            return false;
        }
        case WM_KEYUP:
        case WM_SYSKEYUP: {
            SettingsPage* activePage = page();
            if (activePage != nullptr && activePage->rawKey(msg, w)) {
                result = 0;
                return true;
            }
            return false;
        }
        case kMsgUpdateResult:
            drainResults();
            result = 0;
            return true;
        default:
            return false;
    }
}

bool SettingsWindow::pasteToField()
{
    SettingsPage* activePage = page();
    if (activePage == nullptr || !OpenClipboard(hwnd())) {
        return false;
    }
    bool handled = false;
    HANDLE data = GetClipboardData(CF_UNICODETEXT);
    if (data != nullptr) {
        const auto* text = static_cast<const wchar_t*>(GlobalLock(data));
        if (text != nullptr) {
            std::wstring line;
            for (const wchar_t* c = text; *c != L'\0' && *c != L'\r' && *c != L'\n'; ++c) {
                line.push_back(*c);
            }
            handled = activePage->paste(line);
            GlobalUnlock(data);
        }
    }
    CloseClipboard();
    return handled;
}

// ---------------------------------------------------------------------------
// Entrada
// ---------------------------------------------------------------------------
bool SettingsWindow::onPointer(const PointerEvent& e)
{
    if (e.kind == PointerKind::Down) {
        SetFocus(hwnd());
        layout();
    }

    // Rota do controle capturado ate o Up/Leave.
    if (capture_ != nullptr && e.kind != PointerKind::Down &&
        e.kind != PointerKind::Wheel) {
        Control* target = capture_;
        if (e.kind != PointerKind::Move && e.kind != PointerKind::Leave) {
            capture_ = nullptr;
            releaseMouse();
        }
        target->onPointer(e);
        return true;
    }

    if (e.kind == PointerKind::Move) {
        const size_t hover =
            navRect_.contains(e.x, e.y) ? navHit(e.x, e.y) : kPageCount;
        if (hover != navHover_) {
            navHover_ = hover;
            requestRender();
        }
    }

    if (e.kind == PointerKind::Down && e.button == MouseButton::Left &&
        navRect_.contains(e.x, e.y)) {
        const size_t index = navHit(e.x, e.y);
        if (index < kPageCount) {
            selectPage(index);
        }
        return true;
    }

    if (closeButton_.onPointer(e)) {
        if (e.kind == PointerKind::Down) {
            capture_ = &closeButton_;
            captureMouse();
        }
        return true;
    }

    SettingsPage* activePage = page();
    if (activePage != nullptr) {
        Control* capture = nullptr;
        if (activePage->pointer(e, capture)) {
            if (e.kind == PointerKind::Down && capture != nullptr) {
                capture_ = capture;
                captureMouse();
            }
            return true;
        }
    }
    return false;
}

bool SettingsWindow::onKey(const KeyEvent& e)
{
    SettingsPage* activePage = page();
    if (activePage != nullptr && activePage->key(e)) {
        return true;
    }
    if (e.down && e.key == Key::Escape) {
        hide();
        return true;
    }
    return false;
}

bool SettingsWindow::onChar(wchar_t c)
{
    SettingsPage* activePage = page();
    return activePage != nullptr && activePage->character(c);
}

// ---------------------------------------------------------------------------
// Pintura
// ---------------------------------------------------------------------------
void SettingsWindow::onRender(IRenderTarget& rt, const Rect& client)
{
    (void)client;
    layout();
    const tokens::Palette p = palette();
    rt.clear(p.windowBg);

    // Barra de titulo --------------------------------------------------------
    TextStyle title;
    title.fontFamily = tokens::kFontUi;
    title.fontSize = tokens::kFontSubtitle * scale_;
    title.weight = tokens::kWeightSemibold;
    title.color = p.textPrimary;
    title.align = TextStyle::Align::Left;
    title.valign = TextStyle::VAlign::Middle;
    const Rect titleBox{s(8), 0.f, titleBar_.w - s(8) * 2.f, titleBar_.h};
    rt.drawText(tr(L"settings.title"), titleBox, title);
    rt.drawLine(0.f, titleBar_.h - 1.f, titleBar_.w, titleBar_.h - 1.f, 1.f, p.border);
    closeButton_.render(rt, p);

    // Navegacao lateral ------------------------------------------------------
    rt.fillRoundedRect(navRect_, 0.f, p.surfaceAlt);
    rt.drawLine(navRect_.x + navRect_.w, navRect_.y, navRect_.x + navRect_.w,
                navRect_.y + navRect_.h, 1.f, p.border);

    for (size_t i = 0; i < kPageCount; ++i) {
        const Rect item = navItems_[i];
        const bool isSelected = (i == selected_);
        const bool isHovered = (i == navHover_);
        if (isSelected) {
            rt.fillRoundedRect(item, tokens::kRadiusMd * scale_, p.accent);
        } else if (isHovered) {
            rt.fillRoundedRect(item, tokens::kRadiusMd * scale_, p.hover);
        }

        const Color fg = isSelected ? p.accentFg : p.textPrimary;
        TextStyle glyph;
        glyph.fontFamily = tokens::kFontIcons;
        glyph.fontSize = tokens::kFontBody * scale_;
        glyph.weight = tokens::kWeightRegular;
        glyph.color = fg;
        glyph.align = TextStyle::Align::Center;
        glyph.valign = TextStyle::VAlign::Middle;
        const Rect glyphBox{item.x + s(4), item.y, s(24), item.h};
        rt.drawGlyph(kNavSpecs[i].glyph, glyphBox, glyph);

        TextStyle label;
        label.fontFamily = tokens::kFontUi;
        label.fontSize = tokens::kFontBody * scale_;
        label.weight = isSelected ? tokens::kWeightSemibold : tokens::kWeightRegular;
        label.color = fg;
        label.align = TextStyle::Align::Left;
        label.valign = TextStyle::VAlign::Middle;
        const Rect labelBox{glyphBox.x + glyphBox.w + s(4), item.y,
                            item.w - (glyphBox.w + s(12)), item.h};
        rt.drawText(tr(kNavSpecs[i].key), labelBox, label);
    }

    // Conteudo ----------------------------------------------------------------
    rt.pushClip(contentRect_);
    SettingsPage* activePage = page();
    if (activePage != nullptr) {
        TextStyle heading;
        heading.fontFamily = tokens::kFontUi;
        heading.fontSize = tokens::kFontTitle * scale_;
        heading.weight = tokens::kWeightSemibold;
        heading.color = p.textPrimary;
        heading.align = TextStyle::Align::Left;
        heading.valign = TextStyle::VAlign::Middle;
        rt.drawText(tr(activePage->titleKey()), headingRect_, heading);
        activePage->render(rt, p);
    }
    rt.popClip();
}

void SettingsWindow::onDestroy()
{
    g_hwnd.store(nullptr);
    capture_ = nullptr;
}

}  // namespace (interno: classe janela e estado estatico)

// ---------------------------------------------------------------------------
// Seam publico (declarado em settings_window.h)
// ---------------------------------------------------------------------------
void init(SettingsHost newHost)
{
    hostRef() = std::move(newHost);
}

void show()
{
    SettingsWindow& window = instance();
    if (window.ensureCreated()) {
        window.showCentered();
    }
}

void hide()
{
    instance().hide();
}

bool isVisible()
{
    return instance().isVisible();
}

void shutdown()
{
    SettingsWindow& window = instance();
    // Invalida antes de destruir: um resultado em voo carrega g_hwnd e faz
    // PostMessageW; janela morta ignora, handle reciclado nao.
    g_hwnd.store(nullptr);
    window.hide();
    window.destroy();
    {
        std::lock_guard<std::mutex> lock(g_resultMutex);
        g_results.clear();
    }
    hostRef() = SettingsHost{};
}

SettingsHost& host()
{
    return hostRef();
}

void notifyLanguageChanged()
{
    instance().refreshAndLayout();
}

// ---------------------------------------------------------------------------
// Resultado assincrono do checkForUpdates
// ---------------------------------------------------------------------------
void postUpdateResult(bool found, const std::wstring& info)
{
    {
        std::lock_guard<std::mutex> lock(g_resultMutex);
        g_results.push_back({found, info});
    }
    const HWND hwnd = g_hwnd.load();
    if (hwnd != nullptr) {
        PostMessageW(hwnd, kMsgUpdateResult, 0, 0);
    }
}

bool takeUpdateResult(bool& found, std::wstring& info)
{
    std::lock_guard<std::mutex> lock(g_resultMutex);
    if (g_results.empty()) {
        return false;
    }
    found = g_results.front().found;
    info = g_results.front().info;
    g_results.erase(g_results.begin());
    return true;
}

// ---------------------------------------------------------------------------
// Registro de inicializacao + normalizacao de regras
// ---------------------------------------------------------------------------
std::wstring startupCommandLine(const std::wstring& exePath)
{
    return quoted(exePath);
}

std::wstring startupValue()
{
    wchar_t buffer[MAX_PATH * 2] = L"";
    const DWORD size = static_cast<DWORD>(sizeof(buffer) / sizeof(buffer[0]));
    const DWORD len = GetModuleFileNameW(nullptr, buffer, size);
    if (len == 0 || len >= size) {
        return L"";
    }
    return quoted(std::wstring(buffer, len));
}

bool applyStartup(bool enabled)
{
    const std::wstring value = startupValue();
    if (value.empty()) {
        return false;
    }

    HKEY key = nullptr;
    const LSTATUS created =
        RegCreateKeyExW(HKEY_CURRENT_USER, kRunKey, 0, nullptr, 0,
                        KEY_SET_VALUE | KEY_QUERY_VALUE, nullptr, &key, nullptr);
    if (created != ERROR_SUCCESS) {
        return false;
    }

    bool ok = false;
    if (enabled) {
        const DWORD bytes = static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t));
        ok = RegSetValueExW(key, kRunValueName, 0, REG_SZ,
                            reinterpret_cast<const BYTE*>(value.c_str()), bytes) ==
             ERROR_SUCCESS;
    } else {
        const LSTATUS removed = RegDeleteValueW(key, kRunValueName);
        ok = (removed == ERROR_SUCCESS) || (removed == ERROR_FILE_NOT_FOUND);
    }
    RegCloseKey(key);
    return ok;
}

std::wstring normalizeProcessName(const std::wstring& processName)
{
    static constexpr wchar_t kWs[] = L" \t\r\n";
    const size_t begin = processName.find_first_not_of(kWs);
    if (begin == std::wstring::npos) {
        return L"";
    }
    const size_t end = processName.find_last_not_of(kWs);
    std::wstring out = processName.substr(begin, end - begin + 1);
    for (wchar_t& c : out) {
        if (c >= L'A' && c <= L'Z') {
            c = static_cast<wchar_t>(c - L'A' + L'a');
        }
    }
    return out;
}

}  // namespace soundint::ui::settings

#include "ui/windows/window_base.h"

#include "core/log.h"
#include "core/store.h"
#include "ui/palette.h"

#include <dwmapi.h>
#include <windowsx.h>

#include <algorithm>
#include <vector>

namespace soundint::ui {
namespace {

// Registro de janelas vivas para o invalidador global (requestRender).
std::vector<WindowBase*>& trackedWindows()
{
    static std::vector<WindowBase*> windows;
    return windows;
}

bool gInvalidateInstalled = false;

void globalInvalidate(void*)
{
    for (WindowBase* window : trackedWindows()) {
        if (window->valid() && window->isVisible()) {
            window->invalidate();
        }
    }
}

Key keyFromVk(unsigned vk)
{
    switch (vk) {
        case VK_ESCAPE:  return Key::Escape;
        case VK_RETURN:  return Key::Enter;
        case VK_TAB:     return Key::Tab;
        case VK_BACK:    return Key::Backspace;
        case VK_SPACE:   return Key::Space;
        case VK_LEFT:    return Key::Left;
        case VK_RIGHT:   return Key::Right;
        case VK_UP:      return Key::Up;
        case VK_DOWN:    return Key::Down;
        case VK_HOME:    return Key::Home;
        case VK_END:     return Key::End;
        default:         return Key::None;
    }
}

KeyEvent keyEventFromWp(WPARAM w, bool down)
{
    KeyEvent event;
    event.down = down;
    event.key = keyFromVk(static_cast<unsigned>(w));
    event.ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    event.shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    event.alt = (GetKeyState(VK_MENU) & 0x8000) != 0;
    return event;
}

PointerEvent pointerEventFromMsg(HWND hwnd, UINT msg, WPARAM w, LPARAM l)
{
    PointerEvent event;
    event.x = static_cast<float>(static_cast<short>(LOWORD(l)));
    event.y = static_cast<float>(static_cast<short>(HIWORD(l)));
    switch (msg) {
        case WM_LBUTTONDOWN:
            event.kind = PointerKind::Down;
            event.button = MouseButton::Left;
            break;
        case WM_LBUTTONUP:
            event.kind = PointerKind::Up;
            event.button = MouseButton::Left;
            break;
        case WM_RBUTTONDOWN:
            event.kind = PointerKind::Down;
            event.button = MouseButton::Right;
            break;
        case WM_RBUTTONUP:
            event.kind = PointerKind::Up;
            event.button = MouseButton::Right;
            break;
        case WM_MOUSELEAVE:
            event.kind = PointerKind::Leave;
            break;
        case WM_MOUSEWHEEL: {
            event.kind = PointerKind::Wheel;
            POINT pt{GET_X_LPARAM(l), GET_Y_LPARAM(l)};
            ScreenToClient(hwnd, &pt);
            event.x = static_cast<float>(pt.x);
            event.y = static_cast<float>(pt.y);
            // notches (+ = para cima), como no contrato PointerEvent
            event.wheelDelta = static_cast<float>(GET_WHEEL_DELTA_WPARAM(w)) /
                               static_cast<float>(WHEEL_DELTA);
            break;
        }
        default:
            event.kind = PointerKind::Move;
            break;
    }
    return event;
}

}  // namespace

WindowBase::~WindowBase()
{
    destroy();
}

// ---------------------------------------------------------------------------
// Ciclo de vida
// ---------------------------------------------------------------------------
bool WindowBase::create(HINSTANCE instance, const wchar_t* className, DWORD exStyle,
                        DWORD style)
{
    if (hwnd_ || !instance || !className) {
        return false;
    }

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    if (!GetClassInfoExW(instance, className, &wc)) {
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = &WindowBase::wndProc;
        wc.hInstance = instance;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr;
        wc.lpszClassName = className;
        if (!RegisterClassExW(&wc)) {
            SI_LOG_ERROR("ui", std::wstring(L"WindowBase: RegisterClassExW falhou (") +
                                   className + L", erro " +
                                   std::to_wstring(GetLastError()) + L")");
            return false;
        }
    }

    hwnd_ = CreateWindowExW(exStyle, className, L"", style | WS_CLIPCHILDREN, 0, 0, 100,
                            100, nullptr, nullptr, instance, this);
    if (!hwnd_) {
        SI_LOG_ERROR("ui", std::wstring(L"WindowBase: CreateWindowExW falhou (") +
                               className + L", erro " +
                               std::to_wstring(GetLastError()) + L")");
        return false;
    }

    // Sombra DWM em torno do popup + cantos arredondados no Win11
    // (DWMWA_WINDOW_CORNER_PREFERENCE=33; ignorado no Win10).
    MARGINS margins{-1, -1, -1, -1};
    DwmExtendFrameIntoClientArea(hwnd_, &margins);
    int corner = 2;  // DWMWCP_ROUND
    DwmSetWindowAttribute(hwnd_, 33, &corner, sizeof(corner));

    Renderer::instance().attach(hwnd_);
    trackedWindows().push_back(this);
    if (!gInvalidateInstalled) {
        setInvalidateHandler(&globalInvalidate, nullptr);
        gInvalidateInstalled = true;
    }
    return true;
}

void WindowBase::destroy()
{
    if (hwnd_) {
        DestroyWindow(hwnd_);   // WM_DESTROY limpa hwnd_/detach
    }
}

void WindowBase::requestClose()
{
    if (hwnd_) {
        DestroyWindow(hwnd_);
    }
}

bool WindowBase::isVisible() const
{
    return hwnd_ && IsWindowVisible(hwnd_);
}

// ---------------------------------------------------------------------------
// Posicao / repintura
// ---------------------------------------------------------------------------
void WindowBase::showAt(int x, int y, bool activate)
{
    if (!hwnd_) {
        return;
    }
    SetWindowPos(hwnd_, HWND_TOP, x, y, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
    ShowWindow(hwnd_, activate ? SW_SHOW : SW_SHOWNOACTIVATE);
    if (activate) {
        SetForegroundWindow(hwnd_);
        SetFocus(hwnd_);
    }
    invalidate();
}

void WindowBase::hide()
{
    if (hwnd_) {
        ShowWindow(hwnd_, SW_HIDE);
    }
}

void WindowBase::invalidate()
{
    if (hwnd_) {
        InvalidateRect(hwnd_, nullptr, FALSE);
    }
}

void WindowBase::resizeClient(int width, int height)
{
    if (!hwnd_ || width <= 0 || height <= 0) {
        return;
    }
    RECT rect{0, 0, width, height};
    DWORD style = static_cast<DWORD>(GetWindowLongPtrW(hwnd_, GWL_STYLE));
    DWORD exStyle = static_cast<DWORD>(GetWindowLongPtrW(hwnd_, GWL_EXSTYLE));
    AdjustWindowRectEx(&rect, style, FALSE, exStyle);
    SetWindowPos(hwnd_, nullptr, 0, 0, rect.right - rect.left, rect.bottom - rect.top,
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

Rect WindowBase::clientBounds() const
{
    RECT rect{};
    if (hwnd_) {
        GetClientRect(hwnd_, &rect);
    }
    return {0.f, 0.f, static_cast<float>(rect.right - rect.left),
            static_cast<float>(rect.bottom - rect.top)};
}

float WindowBase::dpiScale() const
{
    return hwnd_ ? windowDpiScale(hwnd_) : 1.f;
}

tokens::Palette WindowBase::palette() const
{
    return resolvePalette(core::Store::instance().settings().theme);
}

bool WindowBase::workAreaAt(POINT pt, RECT& out)
{
    HMONITOR monitor = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
    if (!monitor) {
        return false;
    }
    MONITORINFO info{};
    info.cbSize = sizeof(info);
    if (!GetMonitorInfoW(monitor, &info)) {
        return false;
    }
    out = info.rcWork;
    return true;
}

// ---------------------------------------------------------------------------
// Captura de mouse
// ---------------------------------------------------------------------------
void WindowBase::captureMouse()
{
    if (hwnd_ && GetCapture() != hwnd_) {
        SetCapture(hwnd_);
    }
}

void WindowBase::releaseMouse()
{
    if (hwnd_ && GetCapture() == hwnd_) {
        ReleaseCapture();
    }
}

bool WindowBase::mouseCaptured() const
{
    return hwnd_ && GetCapture() == hwnd_;
}

// ---------------------------------------------------------------------------
// Mensagens
// ---------------------------------------------------------------------------
LRESULT CALLBACK WindowBase::wndProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l)
{
    WindowBase* self = reinterpret_cast<WindowBase*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (msg == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(l);
        self = static_cast<WindowBase*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        if (self) {
            // hwnd_ ainda e nulo aqui; DefWindowProc/WM_SIZE rodam DURANTE a
            // CreateWindowExW — sem este assignment a criacao falha (erro 1400).
            self->hwnd_ = hwnd;
        }
    }
    if (self) {
        return self->handle(msg, w, l);
    }
    return DefWindowProcW(hwnd, msg, w, l);
}

LRESULT WindowBase::handle(UINT msg, WPARAM w, LPARAM l)
{
    // Subclasse tem prioridade (permite WM_CLOSE -> hide, etc.).
    LRESULT result = 0;
    if (handleMessage(msg, w, l, result)) {
        return result;
    }

    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps{};
            HDC dc = ::BeginPaint(hwnd_, &ps);
            (void)dc;
            IRenderTarget* rt = Renderer::instance().beginPaint(hwnd_, &ps.rcPaint);
            if (rt) {
                onRender(*rt, clientBounds());
                Renderer::instance().endPaint(hwnd_);
            }
            ::EndPaint(hwnd_, &ps);
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_SIZE:
            Renderer::instance().resize(hwnd_, LOWORD(l), HIWORD(l));
            onSize(LOWORD(l), HIWORD(l));
            invalidate();
            return 0;
        case WM_DPICHANGED: {
            const RECT* suggested = reinterpret_cast<const RECT*>(l);
            SetWindowPos(hwnd_, nullptr, suggested->left, suggested->top,
                         suggested->right - suggested->left,
                         suggested->bottom - suggested->top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
            invalidate();
            return 0;
        }
        case WM_ACTIVATE:
            onActivate(LOWORD(w) != WA_INACTIVE);
            return 0;
        case WM_KEYDOWN:
            if (onKey(keyEventFromWp(w, true))) {
                return 0;
            }
            break;
        case WM_KEYUP:
            if (onKey(keyEventFromWp(w, false))) {
                return 0;
            }
            break;
        case WM_CHAR:
            if (onChar(static_cast<wchar_t>(w))) {
                return 0;
            }
            break;
        case WM_MOUSEMOVE: {
            TRACKMOUSEEVENT tme{};
            tme.cbSize = sizeof(tme);
            tme.dwFlags = TME_LEAVE;
            tme.hwndTrack = hwnd_;
            TrackMouseEvent(&tme);
            if (onPointer(pointerEventFromMsg(hwnd_, msg, w, l))) {
                return 0;
            }
            break;
        }
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
        case WM_MOUSEWHEEL:
        case WM_MOUSELEAVE:
            if (onPointer(pointerEventFromMsg(hwnd_, msg, w, l))) {
                return 0;
            }
            break;
        case WM_DESTROY: {
            auto& windows = trackedWindows();
            windows.erase(std::remove(windows.begin(), windows.end(), this),
                          windows.end());
            Renderer::instance().detach(hwnd_);
            onDestroy();
            hwnd_ = nullptr;
            return 0;
        }
        default:
            break;
    }
    return DefWindowProcW(hwnd_, msg, w, l);
}

}  // namespace soundint::ui

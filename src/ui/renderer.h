// ============================================================================
// CONTRACT — geometria, cores e eventos de UI. CONGELADO na Wave 0.
// ============================================================================
#pragma once

#include <cmath>
#include <string>
#include <string_view>

#include <windows.h>

namespace soundint::ui {

// ---------------------------------------------------------------------------
// Geometria
// ---------------------------------------------------------------------------
struct Color {
    float r = 0.f, g = 0.f, b = 0.f, a = 1.f;

    static constexpr Color rgb(unsigned char R, unsigned char G, unsigned char B,
                               unsigned char A = 255)
    {
        return {R / 255.f, G / 255.f, B / 255.f, A / 255.f};
    }
};

struct Rect {
    float x = 0.f, y = 0.f, w = 0.f, h = 0.f;

    bool contains(float px, float py) const
    {
        return px >= x && py >= y && px < x + w && py < y + h;
    }
};

struct Size {
    float w = 0.f, h = 0.f;
};

struct TextStyle {
    std::wstring fontFamily;                 // padrao em tokens::kFontUi
    float fontSize = 14.f;
    int weight = 400;                        // 400 regular, 600 semibold
    Color color;
    enum class Align : uint8_t { Left, Center, Right };
    enum class VAlign : uint8_t { Top, Middle, Bottom };
    Align align = Align::Left;
    VAlign valign = VAlign::Middle;
    bool wrap = false;
    bool ellipsis = true;
};

// ---------------------------------------------------------------------------
// Entrada
// ---------------------------------------------------------------------------
enum class PointerKind : uint8_t { Down, Up, Move, Wheel, Leave };
enum class MouseButton : uint8_t { None, Left, Right, Middle };

struct PointerEvent {
    PointerKind kind = PointerKind::Move;
    float x = 0.f, y = 0.f;
    MouseButton button = MouseButton::None;
    float wheelDelta = 0.f;   // notches (+ cima / - baixo)
};

enum class Key : uint8_t {
    None, Escape, Enter, Tab, Backspace, Space,
    Left, Right, Up, Down, Home, End
};

struct KeyEvent {
    bool down = true;
    Key key = Key::None;
    wchar_t character = 0;   // caractere imprimivel (se houver)
    bool ctrl = false, shift = false, alt = false;
};

// ---------------------------------------------------------------------------
// Superficie de desenho (implementada pelo Track D em Direct2D)
// ---------------------------------------------------------------------------
class IRenderTarget {
public:
    virtual ~IRenderTarget() = default;

    virtual void clear(const Color& color) = 0;
    virtual void fillRoundedRect(const Rect& rect, float radius, const Color& color) = 0;
    virtual void strokeRoundedRect(const Rect& rect, float radius, float strokeWidth,
                                   const Color& color) = 0;
    virtual void drawLine(float x1, float y1, float x2, float y2, float width,
                          const Color& color) = 0;
    virtual void drawText(std::wstring_view text, const Rect& box,
                          const TextStyle& style) = 0;
    virtual Size measureText(std::wstring_view, const TextStyle& style) = 0;
    // Glifo de fonte de icones (Segoe Fluent Icons), centralizado em box.
    virtual void drawGlyph(std::wstring_view glyph, const Rect& box,
                           const TextStyle& style) = 0;
    virtual void pushClip(const Rect& rect) = 0;
    virtual void popClip() = 0;
};

// ---------------------------------------------------------------------------
// Ciclo de vida do renderer por janela (Track D implementa)
// ---------------------------------------------------------------------------
class Renderer {
public:
    static Renderer& instance();

    virtual bool initialize() = 0;
    virtual void shutdown() = 0;

    virtual bool attach(HWND hwnd) = 0;
    virtual void detach(HWND hwnd) = 0;
    virtual bool resize(HWND hwnd, unsigned width, unsigned height) = 0;

    // Chamar no WM_PAINT/WM_RENDER. nullptr se a janela nao estiver anexada.
    virtual IRenderTarget* beginPaint(HWND hwnd, const RECT* updateRect) = 0;
    virtual void endPaint(HWND hwnd) = 0;

protected:
    Renderer() = default;
};

// Invalidacao: janelas registram como re-renderizar (ex.: InvalidateRect).
using InvalidateFn = void (*)(void* context);
void setInvalidateHandler(InvalidateFn fn, void* context);
void requestRender();

}  // namespace soundint::ui

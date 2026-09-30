// ============================================================================
// Renderer Direct2D/DirectWrite (Track D).
//
// Implementa soundint::ui::Renderer e a superficie IRenderTarget usadas por
// janelas e controles. O DPI do render target e fixado em 96: todas as
// coordenadas do contrato sao pixels fisicos literais (a escalagem de tokens
// vem na onda 2, via palette::windowDpiScale).
// ============================================================================
#pragma once

#include "ui/renderer.h"

#include <d2d1.h>
#include <dwrite.h>

#include <map>
#include <string>
#include <tuple>
#include <unordered_map>

namespace soundint::ui {

class RendererD2d;

// ---------------------------------------------------------------------------
// ComRef — mini ponteiro inteligente COM (Windows SDK puro, sem WRL).
// ---------------------------------------------------------------------------
template <typename T>
class ComRef {
public:
    ComRef() = default;
    ~ComRef() { reset(); }

    ComRef(const ComRef&) = delete;
    ComRef& operator=(const ComRef&) = delete;

    ComRef(ComRef&& other) noexcept : ptr_(other.ptr_) { other.ptr_ = nullptr; }

    ComRef& operator=(ComRef&& other) noexcept
    {
        if (this != &other) {
            reset();
            ptr_ = other.ptr_;
            other.ptr_ = nullptr;
        }
        return *this;
    }

    T* get() const { return ptr_; }
    T* operator->() const { return ptr_; }
    explicit operator bool() const { return ptr_ != nullptr; }

    // Devolve o endereco interno para a API COM preencher (solta o atual).
    T** put()
    {
        reset();
        return &ptr_;
    }

    // Variante IUnknown** exigida por DWriteCreateFactory.
    IUnknown** putUnknown()
    {
        reset();
        return reinterpret_cast<IUnknown**>(&ptr_);
    }

    void attach(T* ptr)
    {
        reset();
        ptr_ = ptr;
    }

    void reset()
    {
        if (ptr_ != nullptr) {
            ptr_->Release();
            ptr_ = nullptr;
        }
    }

private:
    T* ptr_ = nullptr;
};

// ---------------------------------------------------------------------------
// Cache de IDWriteTextFormat: familia|tamanho|peso|alinhamentos|wrap|ellipsis.
// Preenchido no primeiro uso e invalidado no shutdown().
// ---------------------------------------------------------------------------
struct TextFormatKey {
    std::wstring family;
    float fontSize = 0.f;
    int weight = 400;
    int align = 0;     // DWRITE_TEXT_ALIGNMENT
    int valign = 0;    // DWRITE_PARAGRAPH_ALIGNMENT
    bool wrap = false;
    bool ellipsis = false;

    bool operator<(const TextFormatKey& other) const
    {
        return std::tie(family, fontSize, weight, align, valign, wrap, ellipsis) <
               std::tie(other.family, other.fontSize, other.weight, other.align, other.valign,
                       other.wrap, other.ellipsis);
    }
};

struct TextFormatEntry {
    ComRef<IDWriteTextFormat> format;
    ComRef<IDWriteInlineObject> ellipsisSign;   // mantem o "..." vivo
};

using TextFormatCache = std::map<TextFormatKey, TextFormatEntry>;

// ---------------------------------------------------------------------------
// D2dRenderTarget — wrapper de ID2D1HwndRenderTarget. Nao possui o target
// (a janela e dona, via RendererD2d::WindowState); entre beginPaint/endPaint
// os ponteiros ficam nulos e todo desenho vira no-op seguro.
// ---------------------------------------------------------------------------
class D2dRenderTarget final : public IRenderTarget {
public:
    D2dRenderTarget() = default;
    ~D2dRenderTarget() = default;

    D2dRenderTarget(const D2dRenderTarget&) = delete;
    D2dRenderTarget& operator=(const D2dRenderTarget&) = delete;

    // Liga o wrapper ao target recem-criado (chamado no beginPaint).
    void bind(RendererD2d* owner, ID2D1HwndRenderTarget* target);
    // Solta pincel/estado de clipe apos o EndDraw ou quando o target morre.
    void unbind();

    void clear(const Color& color) override;
    void fillRoundedRect(const Rect& rect, float radius, const Color& color) override;
    void strokeRoundedRect(const Rect& rect, float radius, float strokeWidth,
                           const Color& color) override;
    void drawLine(float x1, float y1, float x2, float y2, float width,
                  const Color& color) override;
    void drawText(std::wstring_view text, const Rect& box, const TextStyle& style) override;
    Size measureText(std::wstring_view text, const TextStyle& style) override;
    void drawGlyph(std::wstring_view glyph, const Rect& box, const TextStyle& style) override;
    void pushClip(const Rect& rect) override;
    void popClip() override;

private:
    bool prepareBrush(const Color& color);

    RendererD2d* owner_ = nullptr;
    ID2D1HwndRenderTarget* target_ = nullptr;
    ComRef<ID2D1SolidColorBrush> brush_;
    int clipDepth_ = 0;
};

// ---------------------------------------------------------------------------
// RendererD2d — singleton concreto de soundint::ui::Renderer.
// Uma entrada por HWND; todas as chamadas de desenho rodam na UI thread.
// ---------------------------------------------------------------------------
class RendererD2d final : public Renderer {
public:
    RendererD2d() = default;
    ~RendererD2d();   // Renderer nao tem dtor virtual: destruir pelo tipo concreto

    bool initialize() override;
    void shutdown() override;
    bool attach(HWND hwnd) override;
    void detach(HWND hwnd) override;
    bool resize(HWND hwnd, unsigned width, unsigned height) override;
    IRenderTarget* beginPaint(HWND hwnd, const RECT* updateRect) override;
    void endPaint(HWND hwnd) override;

private:
    friend class D2dRenderTarget;

    struct WindowState {
        HWND hwnd = nullptr;
        ComRef<ID2D1HwndRenderTarget> target;
        D2dRenderTarget surface;
        bool painting = false;
        bool detached = false;   // detach durante o paint: apagar no endPaint
    };

    WindowState* findWindow(HWND hwnd);
    bool createTarget(WindowState& state);
    void releaseTarget(WindowState& state);
    IDWriteTextFormat* formatFor(const TextStyle& style);

    bool initialized_ = false;
    ComRef<ID2D1Factory> d2dFactory_;
    ComRef<IDWriteFactory> dwriteFactory_;
    TextFormatCache formats_;
    std::unordered_map<HWND, WindowState> windows_;
};

}  // namespace soundint::ui

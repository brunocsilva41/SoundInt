// ============================================================================
// CONTRATO Wave 2 — base de janelas popup (modal, mixer, settings).
// Escrito pelo integrador; subclasses NAO alteram este header.
// Janela top-level WS_POPUP com sombra DWM, cantos arredondados (Win11),
// pintura via Renderer e paleta resolvida do Store.
// Coordenadas de cliente em pixels fisicos; o consumidor escala tokens por
// dpiScale(). Mensagens: handleMessage() roda ANTES dos built-ins — retorne
// true somente para mensagens que a base nao trata (WM_CLOSE etc.); nunca
// intercepte WM_PAINT/WM_SIZE/WM_DESTROY se quiser o comportamento padrao.
// ============================================================================
#pragma once

#include "ui/control.h"
#include "ui/design_tokens.h"
#include "ui/renderer.h"

#include <windows.h>

namespace soundint::ui {

class WindowBase {
public:
    WindowBase() = default;
    WindowBase(const WindowBase&) = delete;
    WindowBase& operator=(const WindowBase&) = delete;
    virtual ~WindowBase();

    // Registra a classe (uma vez por className) e cria a janela top-level.
    // Passa `this` como lpParam. Retorna false em falha.
    bool create(HINSTANCE instance, const wchar_t* className, DWORD exStyle = 0,
                DWORD style = WS_POPUP);

    void destroy();
    HWND hwnd() const { return hwnd_; }
    bool valid() const { return hwnd_ != nullptr; }
    bool isVisible() const;

    // Posiciona (canto superior esquerdo) e mostra. activate=true:
    // foreground + focus; false: SW_SHOWNOACTIVATE (flyout/balao).
    void showAt(int x, int y, bool activate = true);
    void hide();
    void invalidate();

    // Redimensiona a area cliente (px fisicos) — conteudo dinamico
    // (dropdown expandido, paginas) antes de showAt().
    void resizeClient(int width, int height);

    Rect clientBounds() const;
    float dpiScale() const;             // 1.0 = 96 DPI (palette.h)
    tokens::Palette palette() const;    // tema do Store (0=sistema/1/2)

    // Area de trabalho do monitor que contem o ponto.
    static bool workAreaAt(POINT pt, RECT& out);

protected:
    // --- hooks de subclasse ------------------------------------------------
    // Roda antes dos built-ins; true = consumida (nao chama built-ins/DefWP).
    virtual bool handleMessage(UINT /*msg*/, WPARAM /*w*/, LPARAM /*l*/, LRESULT& /*result*/)
    {
        return false;
    }
    // Pintura: rt ja iniciado; cliente em px fisicos. Use palette().
    virtual void onRender(IRenderTarget& rt, const Rect& client) = 0;
    virtual void onDestroy() {}
    virtual void onActivate(bool /*active*/) {}       // WA_INACTIVE -> false
    virtual void onSize(unsigned /*w*/, unsigned /*h*/) {}
    virtual bool onPointer(const PointerEvent&) { return false; }  // traduzida
    virtual bool onKey(const KeyEvent&) { return false; }          // WM_KEYDOWN/UP
    virtual bool onChar(wchar_t) { return false; }                 // WM_CHAR

    // Captura do mouse (rota Move/Up ate ReleaseCapture).
    void captureMouse();
    void releaseMouse();
    bool mouseCaptured() const;

    void requestClose();   // DestroyWindow(hwnd_)

private:
    static LRESULT CALLBACK wndProc(HWND, UINT, WPARAM, LPARAM);
    LRESULT handle(UINT msg, WPARAM w, LPARAM l);

    HWND hwnd_ = nullptr;
};

}  // namespace soundint::ui

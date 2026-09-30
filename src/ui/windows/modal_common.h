// ============================================================================
// Modais popup (Track I) — fila FIFO, posicionamento puro, controles internos
// e seam de integracao (a Wave 3 liga os callbacks de ModalHost em init()).
//
// Regras: UMA janela modal visivel por vez; pedidos simultaneos entram na
// fila (capacidade kQueueCapacity; estourando, descarta o mais antigo) e o
// proximo e exibido quando o atual fechar. init() nao chamado => requests
// sao no-op. Todos os textos passam por ui::tr().
// ============================================================================
#pragma once

#include "core/types.h"
#include "ui/control.h"
#include "ui/controls/interactive.h"
#include "ui/renderer.h"
#include "ui/windows/window_base.h"

#include <cstdint>
#include <deque>
#include <functional>
#include <string>
#include <vector>

#include <windows.h>

namespace soundint::ui::modal {

// ---------------------------------------------------------------------------
// Seam de integracao (Wave 3) — callbacks injetados via init()
// ---------------------------------------------------------------------------
struct ModalHost {
    std::function<std::vector<DeviceInfo>()> outputs;            // endpoints Render ativos
    std::function<std::wstring()> defaultOutput;                 // id do default atual
    std::function<std::wstring(uint32_t pid)> currentAppDevice;  // "" = segue o default
    std::function<bool(const DeviceInfo&)> setDefaultDevice;     // Mult+Console; false = falhou
    std::function<bool(uint32_t pid, const std::wstring& deviceId)> routeApp; // "" limpa
    std::function<void(const ArrivalRule&)> upsertArrival;       // "usar ao conectar"
    std::function<void(const AppRule&)> upsertRule;              // "lembrar deste app"
    std::function<void(const std::wstring& title, const std::wstring& detail)> notify;
};

void init(ModalHost host);
void requestNewDevice(const DeviceInfo& device);   // enfileira; 1 modal visivel por vez
void requestNewApp(const SessionInfo& session);
void shutdown();                                    // fecha e limpa fila
bool isVisible();

// ---------------------------------------------------------------------------
// Posicionamento puro (testado sem HWND)
// ---------------------------------------------------------------------------
inline constexpr float kPopupMargin = 12.f;

// Canto inferior direito da workArea com margem kPopupMargin; o tamanho e
// reduzido quando nao cabe (100% contido na area de trabalho). `anchor` e o
// ponto que define o monitor — o chamador resolve a workArea com
// WindowBase::workAreaAt(anchor); workArea degenerada => ancora no proprio
// anchor (fallback sem chamadas de sistema).
Rect computePopupRect(POINT anchor, Size size, const RECT& workArea);

// ---------------------------------------------------------------------------
// Fila FIFO pura (capacidade kQueueCapacity) — exposta para teste
// ---------------------------------------------------------------------------
inline constexpr size_t kQueueCapacity = 8;

struct PendingModal {
    enum class Kind : uint8_t { Device, App };

    Kind kind = Kind::Device;
    DeviceInfo device{};
    SessionInfo app{};

    static PendingModal forDevice(const DeviceInfo& device);
    static PendingModal forApp(const SessionInfo& session);
};

class ModalQueue {
public:
    // Insere no fim; se cheia, descarta o mais antigo. true = descartou algo.
    bool push(PendingModal item);
    const PendingModal& front() const;   // pre-condicao: !empty()
    void pop();
    void clear();
    bool empty() const { return items_.empty(); }
    size_t size() const { return items_.size(); }
    const std::deque<PendingModal>& items() const { return items_; }

private:
    std::deque<PendingModal> items_;
};

// Fila global do manager (exposta para teste; o fluxo real e
// requestNewDevice/requestNewApp -> showNext()).
ModalQueue& pendingQueue();

// ---------------------------------------------------------------------------
// Utilidades puras (testadas sem HWND)
// ---------------------------------------------------------------------------
// Trim das pontas + colapsa espacos internos em um unico espaco.
std::wstring trimCollapse(std::wstring text);
// Pattern da ArrivalRule (""); vazio quando o nome nao tem conteudo utilizavel.
std::wstring arrivalPatternFrom(const DeviceInfo& device);
// processName normalizado: minusculo e sem espacos (chave da AppRule).
std::wstring normalizeProcessName(std::wstring processName);
// Indice inicial do dropdown de saidas: 0 = "Padrao do sistema".
size_t initialOutputIndex(const std::vector<DeviceInfo>& outputs,
                          const std::wstring& currentDeviceId);
// Glifo do dispositivo novo: headphones quando o nome sugere fone de ouvido.
std::wstring deviceGlyphFor(const std::wstring& friendlyName);

// ---------------------------------------------------------------------------
// Controles internos — o umbrella do Track E nao tem checkbox nem botao
// accent; estilo somente via tokens:: + paleta recebida no render.
// ---------------------------------------------------------------------------
class CheckBox : public controls::InteractiveControl {
public:
    using ChangedFn = std::function<void(bool)>;

    void setText(std::wstring text) { text_ = std::move(text); }
    const std::wstring& text() const { return text_; }
    // Set programatico: nao dispara onChanged.
    void setValue(bool value) { value_ = value; }
    bool value() const { return value_; }
    void setOnChanged(ChangedFn fn) { onChanged_ = std::move(fn); }

    void render(IRenderTarget& rt, const tokens::Palette& palette) override;
    bool onPointer(const PointerEvent& e) override;
    bool onKey(const KeyEvent& e) override;

private:
    void toggle();
    Rect boxRect() const;

    std::wstring text_;
    bool value_ = false;
    ChangedFn onChanged_;

    static constexpr float kBoxSize = 16.f;
};

// Botao primario (fundo accent + texto accentFg).
class AccentButton : public controls::InteractiveControl {
public:
    using ClickFn = std::function<void()>;

    void setText(std::wstring text) { text_ = std::move(text); }
    const std::wstring& text() const { return text_; }
    void setOnClick(ClickFn fn) { onClick_ = std::move(fn); }

    void render(IRenderTarget& rt, const tokens::Palette& palette) override;
    bool onPointer(const PointerEvent& e) override;
    bool onKey(const KeyEvent& e) override;

private:
    void click();

    std::wstring text_;
    ClickFn onClick_;
};

// ---------------------------------------------------------------------------
// Base das janelas modal: roteamento de pointer/foco/teclado e frame
// ---------------------------------------------------------------------------
class ModalWindow : public WindowBase {
public:
    using ClosedFn = std::function<void(ModalWindow*)>;

    void setOnClosed(ClosedFn fn) { onClosed_ = std::move(fn); }
    void setHost(const ModalHost* host) { host_ = host; }

    // Cria (se preciso), posiciona na ancora e mostra em foreground/foco.
    bool present(POINT anchor);
    // Recalcula tamanho/posicao da janela (ex.: dropdown expandido).
    void reframe();

protected:
    ModalWindow() = default;

    // Registra controle: ordem de registro = ordem de foco (Tab) e de desenho.
    void addControl(controls::InteractiveControl& control, bool defaultFocus = false);
    // Reseta foco/captura a cada exibicao (janela reutilizada).
    void resetInteraction();

    // Tamanho do conteudo em px logicos (sem escala) para o frame da janela.
    virtual Size logicalContentSize() const = 0;
    // Posiciona os controles em px fisicos (escala ja aplicada).
    virtual void layoutControls(float scale) = 0;
    virtual void paint(IRenderTarget& rt, float scale, const tokens::Palette& pal) = 0;
    virtual void onPrimaryAction() = 0;    // Enter
    virtual void onCancelAction() = 0;     // Esc / botao secundario
    virtual const wchar_t* windowClassName() const = 0;
    // Fecha popups abertos quando o alvo do Down nao e o proprio controle.
    // true = o Down foi consumido pelo fechamento.
    virtual bool preparePointerDown(controls::InteractiveControl* /*target*/)
    {
        return false;
    }
    // Depois de cada evento: detecta mudanca de expansao e reenquadra.
    virtual void syncState() {}

    const ModalHost* host_ = nullptr;

    bool onPointer(const PointerEvent& e) override;
    bool onKey(const KeyEvent& e) override;
    void onRender(IRenderTarget& rt, const Rect& client) override;
    void onDestroy() override;

private:
    controls::InteractiveControl* hitTest(float x, float y) const;
    void setFocus(controls::InteractiveControl* control);
    void cycleFocus(int direction);
    bool focusable(const controls::InteractiveControl* control) const;

    std::vector<controls::InteractiveControl*> controls_;
    controls::InteractiveControl* captured_ = nullptr;
    controls::InteractiveControl* hovered_ = nullptr;
    controls::InteractiveControl* focused_ = nullptr;
    controls::InteractiveControl* defaultFocus_ = nullptr;
    ClosedFn onClosed_;
    POINT anchor_{};
};

}  // namespace soundint::ui::modal

// ============================================================================
// Mixer flyout da tray (Track J) — janela flutuante de saidas + apps.
//
// API de integracao (Wave 3 liga): init/toggleAt/hide/isVisible/shutdown.
// Os auxiliares puros (computeMixerRect, plano de layout, agrupamento de
// sessoes, itens/selecao do dropdown) ficam neste header para permitir testes
// sem HWND (CI headless).
// ============================================================================
#pragma once

#include "core/event_bus.h"
#include "core/types.h"
#include "ui/renderer.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include <windows.h>

namespace soundint::ui::mixer {

// ---------------------------------------------------------------------------
// Seam de integracao — a Wave 3 injeta o host (endpoints, sessoes, acoes).
// Callbacks ausentes/que falham sao ignorados silenciosamente.
// ---------------------------------------------------------------------------
struct MixerHost {
    std::function<std::vector<DeviceInfo>()> outputs;   // endpoints Render ativos
    std::function<std::wstring()> defaultOutput;        // id do default atual
    std::function<std::vector<SessionInfo>()> sessions; // sessoes Render
    std::function<bool(const std::wstring& deviceId)> setDefault;
    std::function<bool(uint32_t pid, const std::wstring& deviceId)> routeApp; // "" limpa
    std::function<bool(const std::wstring& instanceId, float volume)> setVolume;
    std::function<bool(const std::wstring& instanceId, bool mute)> setMute;
    std::function<void()> openSettings;
};

// Assina o bus (DeviceEvent/SessionEvent) e guarda o host.
// Re-chamado substitui a assinatura anterior (mantem a janela viva).
void init(EventBus& bus, MixerHost host);
void toggleAt(POINT anchor);   // ancora acima/esquerda do ponto; fecha se visivel
void hide();
bool isVisible();
void shutdown();               // fecha a janela e desinscreve do bus

// ---------------------------------------------------------------------------
// Metricas do layout (px logicos). A LARGURA escala por dpiScale(); a altura
// vem dos controles (tokens sem escala — mesmo contrato do Track E).
// ---------------------------------------------------------------------------
namespace layout {
constexpr float kPad = 12.f;         // margem interna da janela
constexpr float kHeaderH = 36.f;     // altura da faixa de cabecalho
constexpr float kHeaderGap = 8.f;    // espaco entre cabecalho e painel
constexpr float kSectionH = 20.f;    // rotulo de secao (capsula)
constexpr float kDividerH = 1.f;     // divisor
constexpr float kRowOutH = 40.f;     // linha de saida (= tokens::kRowHeight)
constexpr float kAppRowH = 80.f;     // linha de app (40 + 4 + slider 32 + 4)
constexpr float kEmptyH = 40.f;      // rotulo de estado vazio
constexpr float kGap = 4.f;          // gap do ListPanel e gap interno das linhas
constexpr float kControlH = 32.f;    // (= tokens::kControlHeight)
constexpr float kWidth = 360.f;      // * dpiScale()
constexpr float kMinWindowH = 96.f;
constexpr float kMaxWorkRatio = 0.7f;   // altura maxima da janela
constexpr float kAnchorDx = 8.f;        // deslocamento da ancora (x)
constexpr float kAnchorDy = 12.f;       // deslocamento da ancora (y)
}  // namespace layout

// ---------------------------------------------------------------------------
// Auxiliares puros (testaveis sem HWND)
// ---------------------------------------------------------------------------

// Retangulo do flyout: acima-esquerda da ancora, 100% contido em `work`.
RECT computeMixerRect(POINT anchor, Size size, const RECT& work);

// Contagem de elementos do painel (alimenta contentHeightFor/planLayout).
struct ContentSpec {
    size_t outputRows = 0;
    size_t appRows = 0;
    size_t idleAppRows = 0;    // secao "Apps abertos" (sessoes paradas)
    bool systemRow = false;
    size_t dropdownItems = 0;   // itens do dropdown de roteamento aberto (0 = fechado)
};

struct LayoutPlan {
    float contentHeight = 0.f;   // altura total do conteudo do painel
    float panelTop = 0.f;        // topo do painel dentro da janela
    float windowHeight = 0.f;    // altura da janela (clamp 70% da work area)
    float viewport = 0.f;        // altura visivel do painel
    float maxScroll = 0.f;       // conteudo - viewport (0 = nao rola)
};

float contentHeightFor(const ContentSpec& spec);
LayoutPlan planLayout(const ContentSpec& spec, float workHeight);

// Ordena as sessoes: ativas primeiro, inativas depois e systemSounds por ultimo.
std::vector<SessionInfo> orderSessions(const std::vector<SessionInfo>& sessions);

// Selecao das linhas do flyout a partir de sessions().
struct SessionPlan {
    std::vector<SessionInfo> apps;      // sessoes ativas, sem systemSounds
    std::vector<SessionInfo> idleApps;  // sessoes abertas mas paradas (listagem p/ rotear)
    bool systemSounds = false;          // ha linha de "Sons do sistema" no fim
    SessionInfo system;                 // valida quando systemSounds == true
};

SessionPlan planSessions(const std::vector<SessionInfo>& sessions);

// Itens do dropdown de roteamento: [0] = padrao do sistema + nomes das saidas.
std::vector<std::wstring> routeItems(const std::vector<DeviceInfo>& outputs);

// Indice inicial do dropdown: 0 = padrao ("" ou id desconhecido), senao i + 1.
size_t routeSelectionIndex(const std::vector<DeviceInfo>& outputs,
                           const std::wstring& deviceId);

// Glifo da saida pelo nome: head/fone/auricul/airpod -> fones, senao speaker.
std::wstring outputGlyphFor(const std::wstring& friendlyName);

}  // namespace soundint::ui::mixer

// ============================================================================
// Helper local do U3 (header-only): linha de status transitoria com desfazer
// + mapeamento das escolhas de saida (item 0 = "Padrao do sistema").
// Consumido apenas pelas paginas de Regras/Perfis/Atalhos e pelos testes do
// U3; nenhum outro track inclui este arquivo.
//
// Timeout SEM polling: um unico timer de thread (SetTimer com TIMERPROC, o
// mesmo padrao do modal_common) limpa os statuses vencidos e pede re-render.
// O relogio e injetavel para os testes unitarios.
// ============================================================================
#pragma once

#include "core/types.h"
#include "ui/design_tokens.h"
#include "ui/renderer.h"   // requestRender()

#include <windows.h>

#include <algorithm>
#include <functional>
#include <limits>
#include <string>
#include <vector>

namespace soundint::ui::settings {

// Cor semantica do status; a pagina resolve com a paleta ativa.
enum class StatusKind { Info, Success, Danger };

inline Color statusColor(StatusKind kind, const tokens::Palette& palette)
{
    switch (kind) {
        case StatusKind::Success:
            return palette.success;
        case StatusKind::Danger:
            return palette.danger;
        case StatusKind::Info:
        default:
            return palette.accent;
    }
}

// ---------------------------------------------------------------------------
// OutputChoices: itens do Dropdown de destino das paginas Regras/Perfis.
// Item 0 = "Padrao do sistema" (deviceId vazio); demais = host().outputs.
// ---------------------------------------------------------------------------
class OutputChoices {
public:
    // Recarrega os itens do seletor a partir das saidas atuais.
    void refresh(std::vector<DeviceInfo> outputs, const wchar_t* defaultLabel)
    {
        outputs_ = std::move(outputs);
        items_.clear();
        items_.emplace_back(defaultLabel != nullptr ? defaultLabel : L"");
        for (const DeviceInfo& device : outputs_) {
            items_.push_back(device.friendlyName);
        }
    }

    // Indice do Dropdown para o deviceId (0 = vazio ou desconhecido).
    size_t indexForId(const std::wstring& deviceId) const
    {
        if (deviceId.empty()) {
            return 0;
        }
        for (size_t i = 0; i < outputs_.size(); ++i) {
            if (_wcsicmp(outputs_[i].id.c_str(), deviceId.c_str()) == 0) {
                return i + 1;   // item 0 = padrao do sistema
            }
        }
        return 0;
    }

    // DeviceId do indice do Dropdown (vazio = padrao do sistema).
    std::wstring idForIndex(size_t index) const
    {
        if (index == 0 || index > outputs_.size()) {
            return std::wstring{};
        }
        return outputs_[index - 1].id;
    }

    // Nome amigavel do dispositivo; vazio => fallbackLabel; desconhecido =>
    // id cru (endpoint offline).
    std::wstring friendlyName(const std::wstring& deviceId,
                              const wchar_t* fallbackLabel) const
    {
        if (deviceId.empty()) {
            return std::wstring(fallbackLabel != nullptr ? fallbackLabel : L"");
        }
        for (const DeviceInfo& device : outputs_) {
            if (_wcsicmp(device.id.c_str(), deviceId.c_str()) == 0) {
                return device.friendlyName;
            }
        }
        return deviceId;
    }

    const std::vector<std::wstring>& items() const { return items_; }

private:
    std::vector<DeviceInfo> outputs_;
    std::vector<std::wstring> items_;
};

// ---------------------------------------------------------------------------
// PageStatus: estado da linha de status de uma pagina (texto + cor semantica
// + desfazer opcional). Cada instancia se registra num inventario global; o
// timer limpa os vencidos sem guardar ponteiro para a pagina.
// ---------------------------------------------------------------------------
class PageStatus {
public:
    static constexpr unsigned kDefaultTimeoutMs = 4000;   // status comum
    static constexpr unsigned kUndoTimeoutMs = 5000;      // status com desfazer

    PageStatus() { registry().push_back(this); }

    ~PageStatus()
    {
        auto& entries = registry();
        entries.erase(std::remove(entries.begin(), entries.end(), this),
                      entries.end());
    }

    PageStatus(const PageStatus&) = delete;
    PageStatus& operator=(const PageStatus&) = delete;

    // Exibe o status por timeoutMs. undo != nulo habilita o botao Desfazer
    // enquanto o status estiver visivel; o callback NAO deve capturar a
    // pagina — apenas o snapshot a restaurar no Store.
    void show(StatusKind kind, std::wstring text,
              unsigned timeoutMs = kDefaultTimeoutMs,
              std::function<void()> undo = nullptr)
    {
        kind_ = kind;
        text_ = std::move(text);
        undo_ = std::move(undo);
        deadline_ = now() + timeoutMs;
        armTimer();
    }

    void clear()
    {
        text_.clear();
        undo_ = nullptr;
    }

    bool active() const { return !text_.empty() && now() < deadline_; }
    bool hasUndo() const { return active() && undo_ != nullptr; }
    StatusKind kind() const { return kind_; }
    const std::wstring& text() const { return text_; }

    // Executa o desfazer (se houver) e limpa o status.
    void runUndo()
    {
        if (!hasUndo()) {
            return;
        }
        std::function<void()> undo = std::move(undo_);
        clear();
        undo();
    }

    // Apenas para testes: injeta o relogio (nullptr restaura GetTickCount64).
    static void setClockForTests(ULONGLONG (*clockFn)()) { clockFn_ = clockFn; }

private:
    // Id do timer de thread compartilhado ("SDNE"; o modal usa 0x534D49).
    static constexpr UINT_PTR kTimerId = 0x53444E45;

    static std::vector<PageStatus*>& registry()
    {
        static std::vector<PageStatus*> entries;
        return entries;
    }

    static ULONGLONG now()
    {
        if (clockFn_ != nullptr) {
            return clockFn_();
        }
        return GetTickCount64();
    }

    // (Re)arma o timer para o prazo mais curto entre os statuses ativos.
    static void armTimer()
    {
        const ULONGLONG kNever = std::numeric_limits<ULONGLONG>::max();
        ULONGLONG earliest = kNever;
        for (const PageStatus* entry : registry()) {
            if (!entry->text_.empty() && entry->deadline_ > now()) {
                earliest = std::min(earliest, entry->deadline_);
            }
        }
        if (earliest == kNever) {
            return;
        }
        ULONGLONG remaining = earliest - now();
        if (remaining > USER_TIMER_MAXIMUM) {
            remaining = USER_TIMER_MAXIMUM;
        }
        SetTimer(nullptr, kTimerId,
                 remaining != 0u ? static_cast<UINT>(remaining) : 1u, &timerProc);
    }

    static VOID CALLBACK timerProc(HWND, UINT, UINT_PTR id, DWORD)
    {
        KillTimer(nullptr, id);
        bool changed = false;
        for (PageStatus* entry : registry()) {
            if (!entry->text_.empty() && now() >= entry->deadline_) {
                entry->clear();
                changed = true;
            }
        }
        armTimer();   // ainda ha status vivo? rearma para o proximo prazo
        if (changed) {
            requestRender();
        }
    }

    StatusKind kind_ = StatusKind::Info;
    std::wstring text_;
    std::function<void()> undo_;
    ULONGLONG deadline_ = 0;

    static ULONGLONG (*clockFn_)();
};

inline ULONGLONG (*PageStatus::clockFn_)() = nullptr;

}  // namespace soundint::ui::settings

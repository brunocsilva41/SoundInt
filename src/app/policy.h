// ============================================================================
// Politica pura de decisao do app-shell (Track H). SEM dependencia de Win32:
// o alvo soundint_tests_app linka apenas `core`. Todo relogio vem injetado em
// milissegundos para permitir testes deterministicos.
// ============================================================================
#pragma once

#include "core/types.h"

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace soundint::policy {

using TimeMs = int64_t;

// Relogio monotono em ms. A politica nao le o relogio por si: as funcoes
// recebem `nowMs` injetado; `now()` existe para o app registrar o instante.
TimeMs now();

// --- texto (ASCII case-insensitive) -----------------------------------------
bool iEqual(const std::wstring& a, const std::wstring& b);
// Padrao vazio NAO casa (evita disparo surpresa em regra mal preenchida).
bool iContains(const std::wstring& haystack, const std::wstring& needle);

// --- decisao ----------------------------------------------------------------
enum class Decision : uint8_t {
    ApplyRuleSilently,  // ha regra habilitada: roteia sem perguntar
    AskPopup,           // pode mostrar o popup agora
    SuppressCooldown,   // queria perguntar, mas ainda esta em cooldown
    Ignore,             // sons do sistema / popup desligado / regra casou
};

bool isSystemSounds(const SessionInfo& session);
bool hasRule(const std::optional<AppRule>& rule);  // existe E habilitada
bool popupEnabled(bool setting);

// ms restantes de cooldown (0 = livre ou nunca usou).
TimeMs cooldownRemaining(TimeMs nowMs, TimeMs lastMs, TimeMs cooldownMs);

// Decisao de popup de um app recem-criado.
Decision decideSession(const SessionInfo& session,
                       const std::optional<AppRule>& rule,
                       bool popupSetting,
                       TimeMs nowMs,
                       TimeMs lastPopupMs,
                       TimeMs cooldownMs);

// Decisao de popup de um dispositivo recem-chegado. `arrivalMatched` = alguma
// regra de chegada casou com o nome do dispositivo (silencia o popup).
Decision decideDevice(bool arrivalMatched,
                      bool popupSetting,
                      TimeMs nowMs,
                      TimeMs lastPopupMs,
                      TimeMs cooldownMs);

// Primeira regra habilitada cujo padrao esta contido no friendly name.
const ArrivalRule* matchArrival(const std::vector<ArrivalRule>& rules,
                                const std::wstring& friendlyName);

// --- janelas de tempo com relogio injetavel ---------------------------------

// Coalesce: cada `poke()` reagenda o deadline para now + interval; a janela
// so dispara quando o relogio injetado alcanca o deadline.
class Debouncer {
public:
    Debouncer() = default;
    explicit Debouncer(TimeMs intervalMs) : intervalMs_(intervalMs) {}

    TimeMs interval() const { return intervalMs_; }
    void setInterval(TimeMs intervalMs) { intervalMs_ = intervalMs; }

    TimeMs poke(const std::wstring& key, TimeMs nowMs);
    TimeMs remaining(const std::wstring& key, TimeMs nowMs) const;
    bool consumeDue(const std::wstring& key, TimeMs nowMs);
    bool has(const std::wstring& key) const;
    void erase(const std::wstring& key);
    void clear();

private:
    TimeMs intervalMs_ = 0;
    std::unordered_map<std::wstring, TimeMs> deadlines_;
};

// Cooldown por chave: guarda o "ultimo popup" e diz quando pode repetir.
class CooldownTracker {
public:
    CooldownTracker() = default;
    explicit CooldownTracker(TimeMs cooldownMs) : cooldownMs_(cooldownMs) {}

    TimeMs cooldown() const { return cooldownMs_; }
    void setCooldown(TimeMs cooldownMs) { cooldownMs_ = cooldownMs; }

    TimeMs last(const std::wstring& key) const;  // 0 = nunca
    TimeMs remaining(const std::wstring& key, TimeMs nowMs) const;
    bool tryAcquire(const std::wstring& key, TimeMs nowMs);  // true = estava livre
    void mark(const std::wstring& key, TimeMs nowMs);
    void erase(const std::wstring& key);
    void clear();

private:
    TimeMs cooldownMs_ = 0;
    std::unordered_map<std::wstring, TimeMs> last_;
};

}  // namespace soundint::policy

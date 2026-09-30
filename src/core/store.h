// ============================================================================
// CONTRACT — persistencia (settings/rules/arrival rules/profiles) em JSON sob
// %LOCALAPPDATA%\SoundInt. Implementacao: Track C (core). Consumidores nao
// devem ler arquivos diretamente; use esta fachada.
// ============================================================================
#pragma once

#include "core/types.h"

#include <optional>
#include <string>
#include <vector>

namespace soundint::core {

class Store {
public:
    static Store& instance();

    // Le os arquivos do disco (cria com padroes se nao existirem).
    bool load();
    // Grava de forma atomica (tmp + rename). Retorna false em falha de IO.
    bool save() const;

    std::wstring dataDir() const;   // %LOCALAPPDATA%\SoundInt
    std::wstring logDir() const;    // %LOCALAPPDATA%\SoundInt\logs

    // Settings (mutavel; chame save() apos alterar).
    Settings& settings();
    const Settings& settings() const;

    // Regras por app (chave: processName minusculo).
    const std::vector<AppRule>& rules() const;
    std::optional<AppRule> findRule(const std::wstring& processName) const;
    void upsertRule(AppRule rule);          // substitui pelo processName
    bool removeRule(const std::wstring& processName);

    // Regras de chegada de dispositivo.
    const std::vector<ArrivalRule>& arrivalRules() const;
    void upsertArrivalRule(ArrivalRule rule);   // substitui pelo pattern
    bool removeArrivalRule(const std::wstring& deviceNamePattern);

    // Perfis.
    const std::vector<Profile>& profiles() const;
    void upsertProfile(Profile profile);    // substitui pelo name
    bool removeProfile(const std::wstring& name);

    // Ultimo dispositivo default persistido pelo app (para reverter).
    std::wstring& lastManualDefaultDevice();

protected:
    Store();
    ~Store();
    struct Impl;
    Impl* impl_;
};

}  // namespace soundint::core

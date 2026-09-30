// ============================================================================
// Codigo interno do Track C: codecs JSON <-> structs da persistencia.
// Nao e contrato publico: consumidores usam soundint::core::Store.
// Persistencia em UTF-8; API publica em wide.
// ============================================================================
#pragma once

#include "core/types.h"

#include <nlohmann/json.hpp>
#include <string>
#include <string_view>
#include <vector>

namespace soundint::core::codec {

// Conversao UTF-8 (arquivo) <-> wide (API do usuario).
std::string toUtf8(std::wstring_view text);
std::wstring fromUtf8(std::string_view text);

// Chave das regras por app: processName em minusculas ASCII.
std::wstring normalizeProcessName(std::wstring_view processName);

// Padroes de Settings (inclui os hotkeys mixer e cycleOutput).
Settings defaultSettings();

// Documento de settings.json: settings + ultimo default manual persistido.
struct SettingsDoc {
    Settings settings;
    std::wstring lastManualDefaultDevice;
};

nlohmann::json settingsToJson(const SettingsDoc& doc);
SettingsDoc settingsFromJson(const nlohmann::json& j);  // fallback: padroes

nlohmann::json rulesToJson(const std::vector<AppRule>& rules);
std::vector<AppRule> rulesFromJson(const nlohmann::json& j);

nlohmann::json arrivalRulesToJson(const std::vector<ArrivalRule>& rules);
std::vector<ArrivalRule> arrivalRulesFromJson(const nlohmann::json& j);

nlohmann::json profilesToJson(const std::vector<Profile>& profiles);
std::vector<Profile> profilesFromJson(const nlohmann::json& j);

}  // namespace soundint::core::codec

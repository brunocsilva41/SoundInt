// Codecs JSON <-> structs. Funcoes puras (sem IO, sem estado): tudo que
// falha com JSON corrompido/invalido cai nos padroes em vez de lancar.
#include "core/store_codec.h"

#include <Windows.h>

#include <utility>

namespace soundint::core::codec {

namespace {

using nlohmann::json;

// ---------------------------------------------------------------------------
// Leitura tolerante de campos opcionais (tipo errado => valor padrao).
// ---------------------------------------------------------------------------
template <typename T>
T optValue(const json& j, const char* key, T fallback)
{
    const auto it = j.find(key);
    if (it == j.end()) {
        return fallback;
    }
    try {
        return it->get<T>();
    } catch (...) {
        return fallback;
    }
}

std::wstring optWide(const json& j, const char* key, const std::wstring& fallback)
{
    const auto it = j.find(key);
    if (it == j.end() || !it->is_string()) {
        return fallback;
    }
    return fromUtf8(it->get_ref<const std::string&>());
}

// String obrigatoria: retorna vazio se ausente/invalida.
std::wstring reqWide(const json& j, const char* key)
{
    return optWide(j, key, std::wstring{});
}

// ---------------------------------------------------------------------------
// Regras por app
// ---------------------------------------------------------------------------
json appRuleToJson(const AppRule& rule)
{
    json j;
    j["enabled"] = rule.enabled;
    j["processName"] = toUtf8(rule.processName);
    j["deviceId"] = toUtf8(rule.deviceId);
    return j;
}

// Retorna false se o elemento e invalido (o chamador pula o item).
bool appRuleFromJson(const json& j, AppRule& out)
{
    if (!j.is_object()) {
        return false;
    }
    std::wstring name = normalizeProcessName(reqWide(j, "processName"));
    if (name.empty()) {
        return false;
    }
    out.enabled = optValue<bool>(j, "enabled", true);
    out.processName = std::move(name);
    out.deviceId = optWide(j, "deviceId", std::wstring{});
    return true;
}

void rulesArrayFromJson(const json& j, std::vector<AppRule>& out)
{
    if (!j.is_array()) {
        return;
    }
    for (const json& item : j) {
        AppRule rule;
        if (appRuleFromJson(item, rule)) {
            out.push_back(std::move(rule));
        }
    }
}

json rulesArrayToJson(const std::vector<AppRule>& rules)
{
    json arr = json::array();
    for (const AppRule& rule : rules) {
        arr.push_back(appRuleToJson(rule));
    }
    return arr;
}

// Raiz dos arquivos de lista: {"version":1,...} ou array puro (tolerante).
const json* listRoot(const json& j, const char* key)
{
    if (j.is_array()) {
        return &j;
    }
    if (j.is_object()) {
        const auto it = j.find(key);
        if (it != j.end()) {
            return &*it;
        }
    }
    return nullptr;
}

json listDoc(const char* key, json&& items)
{
    json j;
    j["version"] = 1;
    j[key] = std::move(items);
    return j;
}

// ---------------------------------------------------------------------------
// Hotkeys
// ---------------------------------------------------------------------------
json hotkeyToJson(const HotkeyBinding& hk)
{
    json j;
    j["enabled"] = hk.enabled;
    j["id"] = toUtf8(hk.id);
    j["modifiers"] = hk.modifiers;
    j["vk"] = hk.vk;
    return j;
}

bool hotkeyFromJson(const json& j, HotkeyBinding& out)
{
    if (!j.is_object()) {
        return false;
    }
    std::wstring id = reqWide(j, "id");
    if (id.empty()) {
        return false;
    }
    out.enabled = optValue<bool>(j, "enabled", true);
    out.id = std::move(id);
    out.modifiers = optValue<unsigned>(j, "modifiers", 0u);
    out.vk = optValue<unsigned>(j, "vk", 0u);
    return true;
}

std::vector<HotkeyBinding> defaultHotkeys()
{
    // Atalhos padrao: mixer (Ctrl+Alt+M) e cycleOutput (Ctrl+Alt+O).
    HotkeyBinding mixer;
    mixer.id = L"mixer";
    mixer.modifiers = 0x0003;  // MOD_CONTROL | MOD_ALT
    mixer.vk = 0x4D;           // 'M'

    HotkeyBinding cycle;
    cycle.id = L"cycleOutput";
    cycle.modifiers = 0x0003;  // MOD_CONTROL | MOD_ALT
    cycle.vk = 0x4F;           // 'O'

    return { mixer, cycle };
}

}  // namespace

// ---------------------------------------------------------------------------
// UTF-8 <-> wide
// ---------------------------------------------------------------------------
std::string toUtf8(std::wstring_view text)
{
    if (text.empty()) {
        return {};
    }
    const int needed = WideCharToMultiByte(CP_UTF8, 0, text.data(),
                                           static_cast<int>(text.size()), nullptr, 0,
                                           nullptr, nullptr);
    if (needed <= 0) {
        return {};
    }
    std::string out(static_cast<size_t>(needed), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                        out.data(), needed, nullptr, nullptr);
    return out;
}

std::wstring fromUtf8(std::string_view text)
{
    if (text.empty()) {
        return {};
    }
    const int needed =
        MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                            nullptr, 0);
    if (needed <= 0) {
        return {};
    }
    std::wstring out(static_cast<size_t>(needed), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                        out.data(), needed);
    return out;
}

std::wstring normalizeProcessName(std::wstring_view processName)
{
    std::wstring out(processName);
    for (wchar_t& c : out) {
        if (c >= L'A' && c <= L'Z') {
            c = static_cast<wchar_t>(c - L'A' + L'a');
        }
    }
    return out;
}

// ---------------------------------------------------------------------------
// Settings
// ---------------------------------------------------------------------------
Settings defaultSettings()
{
    Settings settings;
    settings.hotkeys = defaultHotkeys();
    return settings;
}

nlohmann::json settingsToJson(const SettingsDoc& doc)
{
    const Settings& s = doc.settings;

    json settingsJson;
    settingsJson["popupOnNewDevice"] = s.popupOnNewDevice;
    settingsJson["popupOnNewApp"] = s.popupOnNewApp;
    settingsJson["rememberNewAppChoice"] = s.rememberNewAppChoice;
    settingsJson["startWithWindows"] = s.startWithWindows;
    settingsJson["closeToTray"] = s.closeToTray;
    settingsJson["autoCheckUpdates"] = s.autoCheckUpdates;
    settingsJson["betaChannel"] = s.betaChannel;
    settingsJson["language"] = toUtf8(s.language);
    settingsJson["theme"] = s.theme;

    json hotkeys = json::array();
    for (const HotkeyBinding& hk : s.hotkeys) {
        hotkeys.push_back(hotkeyToJson(hk));
    }
    settingsJson["hotkeys"] = std::move(hotkeys);

    json root;
    root["version"] = 1;
    root["settings"] = std::move(settingsJson);
    root["lastManualDefaultDevice"] = toUtf8(doc.lastManualDefaultDevice);
    return root;
}

SettingsDoc settingsFromJson(const nlohmann::json& j)
{
    // Base sempre valida: se o JSON for lixo, sai igual ao padrao.
    SettingsDoc doc;
    doc.settings = defaultSettings();
    if (!j.is_object()) {
        return doc;
    }

    // Campos dentro de "settings"; se ausente, cai nos padroes ja carregados.
    const auto settingsIt = j.find("settings");
    if (settingsIt != j.end() && settingsIt->is_object()) {
        const json& s = *settingsIt;
        doc.settings.popupOnNewDevice = optValue<bool>(s, "popupOnNewDevice", true);
        doc.settings.popupOnNewApp = optValue<bool>(s, "popupOnNewApp", true);
        doc.settings.rememberNewAppChoice =
            optValue<bool>(s, "rememberNewAppChoice", true);
        doc.settings.startWithWindows = optValue<bool>(s, "startWithWindows", false);
        doc.settings.closeToTray = optValue<bool>(s, "closeToTray", true);
        doc.settings.autoCheckUpdates = optValue<bool>(s, "autoCheckUpdates", true);
        doc.settings.betaChannel = optValue<bool>(s, "betaChannel", false);
        doc.settings.language = optWide(s, "language", doc.settings.language);
        doc.settings.theme = optValue<int>(s, "theme", 0);

        doc.settings.hotkeys.clear();
        const auto hotkeysIt = s.find("hotkeys");
        if (hotkeysIt != s.end() && hotkeysIt->is_array()) {
            for (const json& item : *hotkeysIt) {
                HotkeyBinding hk;
                if (hotkeyFromJson(item, hk)) {
                    doc.settings.hotkeys.push_back(std::move(hk));
                }
            }
        }
        // Arquivo sem hotkeys (ou invalido) recebe os padroes.
        if (doc.settings.hotkeys.empty()) {
            doc.settings.hotkeys = defaultHotkeys();
        }
    }

    doc.lastManualDefaultDevice =
        optWide(j, "lastManualDefaultDevice", std::wstring{});
    return doc;
}

// ---------------------------------------------------------------------------
// Regras por app
// ---------------------------------------------------------------------------
nlohmann::json rulesToJson(const std::vector<AppRule>& rules)
{
    return listDoc("rules", rulesArrayToJson(rules));
}

std::vector<AppRule> rulesFromJson(const nlohmann::json& j)
{
    std::vector<AppRule> out;
    if (const json* root = listRoot(j, "rules")) {
        rulesArrayFromJson(*root, out);
    }
    return out;
}

// ---------------------------------------------------------------------------
// Regras de chegada de dispositivo
// ---------------------------------------------------------------------------
nlohmann::json arrivalRulesToJson(const std::vector<ArrivalRule>& rules)
{
    json arr = json::array();
    for (const ArrivalRule& rule : rules) {
        json j;
        j["enabled"] = rule.enabled;
        j["deviceNamePattern"] = toUtf8(rule.deviceNamePattern);
        j["setAsDefault"] = rule.setAsDefault;
        j["applyRules"] = rulesArrayToJson(rule.applyRules);
        arr.push_back(std::move(j));
    }
    return listDoc("arrivalRules", std::move(arr));
}

std::vector<ArrivalRule> arrivalRulesFromJson(const nlohmann::json& j)
{
    std::vector<ArrivalRule> out;
    const json* root = listRoot(j, "arrivalRules");
    if (root == nullptr || !root->is_array()) {
        return out;
    }
    for (const json& item : *root) {
        if (!item.is_object()) {
            continue;
        }
        std::wstring pattern = reqWide(item, "deviceNamePattern");
        if (pattern.empty()) {
            continue;
        }
        ArrivalRule rule;
        rule.enabled = optValue<bool>(item, "enabled", true);
        rule.deviceNamePattern = std::move(pattern);
        rule.setAsDefault = optValue<bool>(item, "setAsDefault", false);
        const auto applyIt = item.find("applyRules");
        if (applyIt != item.end()) {
            rulesArrayFromJson(*applyIt, rule.applyRules);
        }
        out.push_back(std::move(rule));
    }
    return out;
}

// ---------------------------------------------------------------------------
// Perfis
// ---------------------------------------------------------------------------
nlohmann::json profilesToJson(const std::vector<Profile>& profiles)
{
    json arr = json::array();
    for (const Profile& profile : profiles) {
        json j;
        j["name"] = toUtf8(profile.name);
        j["defaultDeviceId"] = toUtf8(profile.defaultDeviceId);
        j["communicationsDeviceId"] = toUtf8(profile.communicationsDeviceId);
        j["overrides"] = rulesArrayToJson(profile.overrides);
        arr.push_back(std::move(j));
    }
    return listDoc("profiles", std::move(arr));
}

std::vector<Profile> profilesFromJson(const nlohmann::json& j)
{
    std::vector<Profile> out;
    const json* root = listRoot(j, "profiles");
    if (root == nullptr || !root->is_array()) {
        return out;
    }
    for (const json& item : *root) {
        if (!item.is_object()) {
            continue;
        }
        std::wstring name = reqWide(item, "name");
        if (name.empty()) {
            continue;
        }
        Profile profile;
        profile.name = std::move(name);
        profile.defaultDeviceId = optWide(item, "defaultDeviceId", std::wstring{});
        profile.communicationsDeviceId =
            optWide(item, "communicationsDeviceId", std::wstring{});
        const auto overridesIt = item.find("overrides");
        if (overridesIt != item.end()) {
            rulesArrayFromJson(*overridesIt, profile.overrides);
        }
        out.push_back(std::move(profile));
    }
    return out;
}

}  // namespace soundint::core::codec

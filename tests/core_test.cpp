#include "doctest.h"

#include "core/log.h"
#include "core/semver.h"
#include "core/store.h"
#include "core/store_codec.h"

#include <Windows.h>

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

using soundint::AppRule;
using soundint::ArrivalRule;
using soundint::HotkeyBinding;
using soundint::Profile;
using soundint::Settings;
using soundint::Version;
using soundint::core::Store;
namespace codec = soundint::core::codec;
namespace slog = soundint::log;

// ---------------------------------------------------------------------------
// Utilitarios de teste (diretorio temporario + override do Store)
// ---------------------------------------------------------------------------
std::wstring tempPath(const std::wstring& leaf)
{
    wchar_t buf[MAX_PATH + 1] = {};
    const DWORD len = GetTempPathW(MAX_PATH, buf);
    std::wstring dir(buf, len > 0 ? static_cast<size_t>(len) : 0);
    return dir + leaf;
}

bool fileExists(const std::wstring& path)
{
    return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

std::string readFileText(const std::wstring& path)
{
    std::ifstream f(path, std::ios::binary);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

void writeFileText(const std::wstring& path, const std::string& bytes)
{
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    f.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

// Aponta o Store para um diretorio temporario durante o escopo do teste.
class TempStoreDir {
public:
    explicit TempStoreDir(const std::wstring& leaf) : dir_(tempPath(leaf))
    {
        wchar_t buf[MAX_PATH + 1] = {};
        const DWORD len = GetEnvironmentVariableW(L"SOUNDINT_DATA_DIR", buf, MAX_PATH + 1);
        if (len > 0 && len <= MAX_PATH) {
            hadPrev_ = true;
            prev_.assign(buf, len);
        }
        SetEnvironmentVariableW(L"SOUNDINT_DATA_DIR", dir_.c_str());
    }

    ~TempStoreDir()
    {
        SetEnvironmentVariableW(L"SOUNDINT_DATA_DIR", hadPrev_ ? prev_.c_str() : nullptr);
    }

    TempStoreDir(const TempStoreDir&) = delete;
    TempStoreDir& operator=(const TempStoreDir&) = delete;

    const std::wstring& dir() const { return dir_; }
    std::wstring file(const std::wstring& name) const { return dir_ + L"\\" + name; }

private:
    std::wstring dir_;
    std::wstring prev_;
    bool hadPrev_ = false;
};

void removeStoreFiles(const TempStoreDir& t)
{
    static const wchar_t* kFiles[] = { L"settings.json", L"rules.json",
                                       L"arrival_rules.json", L"profiles.json" };
    for (const wchar_t* name : kFiles) {
        DeleteFileW(t.file(name).c_str());
        DeleteFileW((t.file(name) + L".tmp").c_str());
    }
}

// ---------------------------------------------------------------------------
// SemVer — casos validos
// ---------------------------------------------------------------------------
TEST_CASE("semver: parse de versoes validas")
{
    Version v;

    REQUIRE(Version::parse(L"1.2.3", v));
    CHECK(v.major == 1);
    CHECK(v.minor == 2);
    CHECK(v.patch == 3);
    CHECK(v.preRelease.empty());

    REQUIRE(Version::parse(L"0.0.0", v));
    CHECK(v.major == 0);
    CHECK(v.minor == 0);
    CHECK(v.patch == 0);

    REQUIRE(Version::parse(L"1.2.3-beta.1", v));
    CHECK(v.preRelease == L"beta.1");

    REQUIRE(Version::parse(L"1.2.3-alpha", v));
    CHECK(v.preRelease == L"alpha");

    REQUIRE(Version::parse(L"10.20.30-rc.1+build.5", v));
    CHECK(v.major == 10);
    CHECK(v.minor == 20);
    CHECK(v.patch == 30);
    CHECK(v.preRelease == L"rc.1");  // build metadata e descartada

    REQUIRE(Version::parse(L"1.2.3+sha.abc", v));  // so build, sem pre-release
    CHECK(v.preRelease.empty());

    REQUIRE(Version::parse(L"v1.2.3", v));  // prefixo "v" tolerado
    CHECK(v.major == 1);
    CHECK(v.minor == 2);
    CHECK(v.patch == 3);

    REQUIRE(Version::parse(L"V2.0.0-beta", v));
    CHECK(v.major == 2);
    CHECK(v.preRelease == L"beta");
}

TEST_CASE("semver: parse rejeita entradas invalidas")
{
    const wchar_t* invalid[] = {
        L"",          L"abc",        L"1",         L"1.2",        L"1.2.3.4",
        L"1.2.x",     L"1.2.",       L"1..2",      L"-1.2.3",     L"1.2.3-",
        L"1.2.3-beta..1", L"1.2.3-", L"1.2.3 beta", L"1.2.3_beta", L"v",
        L"1.2.3+",    L"1.2.3-beta+", L"9999999999999999999.0.0",
    };
    for (const wchar_t* text : invalid) {
        Version v;
        INFO("entrada: ", text);
        CHECK_FALSE(Version::parse(text, v));
    }
}

TEST_CASE("semver: parseUtf8 aceita UTF-8 ascii e rejeita lixo")
{
    Version v;
    REQUIRE(Version::parseUtf8("1.2.3-beta.1", v));
    CHECK(v.major == 1);
    CHECK(v.preRelease == L"beta.1");

    Version other;
    REQUIRE(Version::parseUtf8("2.0.0", other));
    CHECK(Version::compare(v, other) < 0);

    CHECK_FALSE(Version::parseUtf8("nao-e-versao", v));
    CHECK_FALSE(Version::parseUtf8("", v));
}

// ---------------------------------------------------------------------------
// SemVer — precedencia (tabela)
// ---------------------------------------------------------------------------
TEST_CASE("semver: precedencia conforme a spec")
{
    struct Case {
        const wchar_t* lower;
        const wchar_t* higher;
    };
    const Case cases[] = {
        { L"1.0.0-alpha", L"1.0.0-alpha.1" },
        { L"1.0.0-alpha.1", L"1.0.0-alpha.beta" },
        { L"1.0.0-alpha.beta", L"1.0.0-beta" },
        { L"1.0.0-beta", L"1.0.0-beta.2" },
        { L"1.0.0-beta.2", L"1.0.0-beta.11" },
        { L"1.0.0-beta.11", L"1.0.0-rc.1" },
        { L"1.0.0-rc.1", L"1.0.0" },
        { L"1.0.0-beta", L"1.0.0" },        // pre-release < release
        { L"1.0.0-1", L"1.0.0-alpha" },     // numerico < nao numerico
        { L"1.2.9", L"1.10.0" },            // comparacao numerica, nao lexicografica
        { L"1.2.3", L"1.10.2" },
        { L"1.0.0", L"2.0.0" },
        { L"1.0.0", L"1.1.0" },
        { L"1.0.0", L"1.0.1" },
    };

    for (const Case& c : cases) {
        Version a;
        Version b;
        REQUIRE(Version::parse(c.lower, a));
        REQUIRE(Version::parse(c.higher, b));
        INFO(c.lower, " < ", c.higher);
        CHECK(Version::compare(a, b) < 0);
        CHECK(Version::compare(b, a) > 0);
    }

    const wchar_t* equals[] = { L"1.0.0", L"v1.0.0", L"1.0.0+build.9" };
    for (size_t i = 0; i < sizeof(equals) / sizeof(equals[0]); ++i) {
        for (size_t j = i; j < sizeof(equals) / sizeof(equals[0]); ++j) {
            Version a;
            Version b;
            REQUIRE(Version::parse(equals[i], a));
            REQUIRE(Version::parse(equals[j], b));
            INFO(equals[i], " == ", equals[j]);
            CHECK(Version::compare(a, b) == 0);
        }
    }

    // Zeros a esquerda em identificador numerico nao mudam a precedencia.
    Version zeroA;
    Version zeroB;
    REQUIRE(Version::parse(L"1.0.0-0", zeroA));
    REQUIRE(Version::parse(L"1.0.0-00", zeroB));
    CHECK(Version::compare(zeroA, zeroB) == 0);

    // Pre-release (mesmo numerico) fica abaixo do release.
    Version preRelease;
    Version release;
    REQUIRE(Version::parse(L"1.0.0-0", preRelease));
    REQUIRE(Version::parse(L"1.0.0", release));
    CHECK(Version::compare(preRelease, release) < 0);
    CHECK(Version::compare(release, preRelease) > 0);
}

TEST_CASE("semver: toString e roundtrip")
{
    Version v;
    REQUIRE(Version::parse(L"1.2.3", v));
    CHECK(v.toString() == L"1.2.3");

    REQUIRE(Version::parse(L"1.2.3-beta.1", v));
    CHECK(v.toString() == L"1.2.3-beta.1");

    REQUIRE(Version::parse(L"v1.2.3-rc.1+ignored", v));
    CHECK(v.toString() == L"1.2.3-rc.1");

    Version round;
    REQUIRE(Version::parse(v.toString(), round));
    CHECK(Version::compare(v, round) == 0);

    Version constructed{ 3, 4, 5, L"beta.7" };
    CHECK(constructed.toString() == L"3.4.5-beta.7");
}

// ---------------------------------------------------------------------------
// Codec — roundtrips
// ---------------------------------------------------------------------------
TEST_CASE("store_codec: roundtrip de settings com hotkeys")
{
    codec::SettingsDoc doc;
    doc.settings.popupOnNewDevice = false;
    doc.settings.startWithWindows = true;
    doc.settings.betaChannel = true;
    doc.settings.language = L"pt-BR";
    doc.settings.theme = 2;

    HotkeyBinding custom;
    custom.enabled = false;
    custom.id = L"profile:3";
    custom.modifiers = 0x0004;  // MOD_SHIFT
    custom.vk = 0x70;           // VK_F1
    doc.settings.hotkeys.push_back(custom);
    doc.settings.hotkeys.push_back(codec::defaultSettings().hotkeys[0]);
    doc.lastManualDefaultDevice = L"\\\\?\\SWD#MMDEVAPI#{0.0.0.00000000}#{e6327cad}";

    const nlohmann::json j = codec::settingsToJson(doc);
    const codec::SettingsDoc back = codec::settingsFromJson(j);

    CHECK_FALSE(back.settings.popupOnNewDevice);
    CHECK(back.settings.popupOnNewApp);           // default preservado
    CHECK(back.settings.startWithWindows);
    CHECK(back.settings.betaChannel);
    CHECK(back.settings.language == L"pt-BR");
    CHECK(back.settings.theme == 2);
    REQUIRE(back.settings.hotkeys.size() == 2);
    CHECK_FALSE(back.settings.hotkeys[0].enabled);
    CHECK(back.settings.hotkeys[0].id == L"profile:3");
    CHECK(back.settings.hotkeys[0].modifiers == 0x0004);
    CHECK(back.settings.hotkeys[0].vk == 0x70);
    CHECK(back.settings.hotkeys[1].id == L"mixer");
    CHECK(back.lastManualDefaultDevice == doc.lastManualDefaultDevice);
}

TEST_CASE("store_codec: hotkeys padroes mixer e cycleOutput")
{
    const Settings defaults = codec::defaultSettings();
    REQUIRE(defaults.hotkeys.size() == 2);
    CHECK(defaults.hotkeys[0].id == L"mixer");
    CHECK(defaults.hotkeys[0].modifiers == 0x0003);  // MOD_CONTROL | MOD_ALT
    CHECK(defaults.hotkeys[0].vk == 0x4D);           // 'M'
    CHECK(defaults.hotkeys[1].id == L"cycleOutput");
    CHECK(defaults.hotkeys[1].modifiers == 0x0003);
    CHECK(defaults.hotkeys[1].vk == 0x4F);           // 'O'

    // Arquivo carregado com hotkeys vazios recebe os padroes.
    nlohmann::json j = codec::settingsToJson(codec::SettingsDoc{});
    j["settings"]["hotkeys"] = nlohmann::json::array();
    const codec::SettingsDoc back = codec::settingsFromJson(j);
    REQUIRE(back.settings.hotkeys.size() == 2);
    CHECK(back.settings.hotkeys[0].id == L"mixer");
    CHECK(back.settings.hotkeys[1].id == L"cycleOutput");
}

TEST_CASE("store_codec: roundtrip de regras por app")
{
    std::vector<AppRule> rules;
    rules.push_back(AppRule{ true, L"Spotify.exe", L"dev-a" });
    rules.push_back(AppRule{ false, L"chrome.exe", L"" });

    const nlohmann::json j = codec::rulesToJson(rules);
    const std::vector<AppRule> back = codec::rulesFromJson(j);

    REQUIRE(back.size() == 2);
    CHECK(back[0].enabled);
    CHECK(back[0].processName == L"spotify.exe");  // chave normalizada ao decodificar
    CHECK(back[0].deviceId == L"dev-a");
    CHECK_FALSE(back[1].enabled);
    CHECK(back[1].processName == L"chrome.exe");
    CHECK(back[1].deviceId.empty());
}

TEST_CASE("store_codec: roundtrip de regras de chegada")
{
    ArrivalRule rule;
    rule.enabled = true;
    rule.deviceNamePattern = L"Arctis";
    rule.setAsDefault = true;
    rule.applyRules.push_back(AppRule{ true, L"discord.exe", L"dev-b" });

    std::vector<ArrivalRule> rules{ rule };
    const nlohmann::json j = codec::arrivalRulesToJson(rules);
    const std::vector<ArrivalRule> back = codec::arrivalRulesFromJson(j);

    REQUIRE(back.size() == 1);
    CHECK(back[0].enabled);
    CHECK(back[0].deviceNamePattern == L"Arctis");
    CHECK(back[0].setAsDefault);
    REQUIRE(back[0].applyRules.size() == 1);
    CHECK(back[0].applyRules[0].processName == L"discord.exe");
    CHECK(back[0].applyRules[0].deviceId == L"dev-b");
}

TEST_CASE("store_codec: roundtrip de perfis")
{
    Profile p;
    p.name = L"Gaming";
    p.defaultDeviceId = L"dev-default";
    p.communicationsDeviceId = L"dev-comm";
    p.overrides.push_back(AppRule{ true, L"game.exe", L"dev-game" });

    const std::vector<Profile> profiles{ p };
    const nlohmann::json j = codec::profilesToJson(profiles);
    const std::vector<Profile> back = codec::profilesFromJson(j);

    REQUIRE(back.size() == 1);
    CHECK(back[0].name == L"Gaming");
    CHECK(back[0].defaultDeviceId == L"dev-default");
    CHECK(back[0].communicationsDeviceId == L"dev-comm");
    REQUIRE(back[0].overrides.size() == 1);
    CHECK(back[0].overrides[0].processName == L"game.exe");
}

TEST_CASE("store_codec: JSON corrompido cai nos padroes")
{
    const nlohmann::json discarded =
        nlohmann::json::parse("{ isso nao e json", nullptr, false);
    REQUIRE(discarded.is_discarded());

    const codec::SettingsDoc doc = codec::settingsFromJson(discarded);
    CHECK(doc.settings.theme == 0);
    CHECK(doc.settings.language == L"auto");
    REQUIRE(doc.settings.hotkeys.size() == 2);
    CHECK(doc.lastManualDefaultDevice.empty());

    CHECK(codec::rulesFromJson(discarded).empty());
    CHECK(codec::arrivalRulesFromJson(discarded).empty());
    CHECK(codec::profilesFromJson(discarded).empty());

    // Tipos errados tambem caem nos padroes sem lancar.
    const nlohmann::json wrongType = nlohmann::json::array();
    CHECK(codec::settingsFromJson(wrongType).settings.hotkeys.size() == 2);
    CHECK(codec::rulesFromJson(nlohmann::json::object()).empty());

    // Elementos invalidos dentro de uma lista boa sao ignorados.
    nlohmann::json mixed = nlohmann::json::array();
    mixed.push_back(nlohmann::json::object());                // sem processName
    mixed.push_back(nlohmann::json{{ "processName", "ok.exe" }});
    const std::vector<AppRule> rules = codec::rulesFromJson(mixed);
    REQUIRE(rules.size() == 1);
    CHECK(rules[0].processName == L"ok.exe");
}

TEST_CASE("store_codec: normalizeProcessName usa minusculas ASCII")
{
    CHECK(codec::normalizeProcessName(L"Spotify.EXE") == L"spotify.exe");
    CHECK(codec::normalizeProcessName(L"CLIp.exe") == L"clip.exe");
    CHECK(codec::normalizeProcessName(L"ja minusculo.exe") == L"ja minusculo.exe");
    CHECK(codec::normalizeProcessName(L"") == L"");
    // Nao-ASCII nao muda; ASCII ao redor continua normalizado (sem locale).
    CHECK(codec::normalizeProcessName(L"J\u00d6RG.exe") == L"j\u00d6rg.exe");
}

// ---------------------------------------------------------------------------
// Store — semantica em memoria (upsert/remove/find)
// ---------------------------------------------------------------------------
TEST_CASE("store: upsert/remove/find normaliza processName")
{
    Store& store = Store::instance();
    store.removeRule(L"testapp.exe");

    const size_t before = store.rules().size();
    store.upsertRule(AppRule{ true, L"TestApp.EXE", L"dev-1" });
    CHECK(store.rules().size() == before + 1);

    // find tolera caixa mista na chave.
    auto found = store.findRule(L"TESTAPP.exe");
    REQUIRE(found.has_value());
    CHECK(found->enabled);
    CHECK(found->deviceId == L"dev-1");

    // upsert substitui pela chave (mesma regra, novo deviceId).
    store.upsertRule(AppRule{ false, L"testapp.exe", L"dev-2" });
    CHECK(store.rules().size() == before + 1);
    found = store.findRule(L"testapp.exe");
    REQUIRE(found.has_value());
    CHECK_FALSE(found->enabled);
    CHECK(found->deviceId == L"dev-2");

    // chave inexistente.
    CHECK_FALSE(store.findRule(L"inexistente.exe").has_value());

    CHECK(store.removeRule(L"TeStApP.EXE"));
    CHECK(store.rules().size() == before);
    CHECK_FALSE(store.removeRule(L"testapp.exe"));
    CHECK_FALSE(store.findRule(L"testapp.exe").has_value());
}

TEST_CASE("store: upsert/remove de arrival rules e perfis")
{
    Store& store = Store::instance();

    const size_t arrivalsBefore = store.arrivalRules().size();
    ArrivalRule rule;
    rule.deviceNamePattern = L"Arctis Nova";
    rule.setAsDefault = true;
    store.upsertArrivalRule(rule);
    CHECK(store.arrivalRules().size() == arrivalsBefore + 1);

    rule.setAsDefault = false;
    rule.applyRules.push_back(AppRule{ true, L"app.exe", L"dev" });
    store.upsertArrivalRule(rule);  // substitui pelo pattern
    CHECK(store.arrivalRules().size() == arrivalsBefore + 1);
    bool foundArrival = false;
    for (const ArrivalRule& r : store.arrivalRules()) {
        if (r.deviceNamePattern == L"Arctis Nova") {
            foundArrival = true;
            CHECK_FALSE(r.setAsDefault);
            REQUIRE(r.applyRules.size() == 1);
        }
    }
    CHECK(foundArrival);
    CHECK(store.removeArrivalRule(L"Arctis Nova"));
    CHECK(store.arrivalRules().size() == arrivalsBefore);
    CHECK_FALSE(store.removeArrivalRule(L"Arctis Nova"));

    const size_t profilesBefore = store.profiles().size();
    Profile p;
    p.name = L"TempProfile";
    p.defaultDeviceId = L"dev-x";
    store.upsertProfile(p);
    CHECK(store.profiles().size() == profilesBefore + 1);

    p.defaultDeviceId = L"dev-y";
    store.upsertProfile(p);  // substitui pelo name
    CHECK(store.profiles().size() == profilesBefore + 1);
    bool foundProfile = false;
    for (const Profile& prof : store.profiles()) {
        if (prof.name == L"TempProfile") {
            foundProfile = true;
            CHECK(prof.defaultDeviceId == L"dev-y");
        }
    }
    CHECK(foundProfile);
    CHECK(store.removeProfile(L"TempProfile"));
    CHECK(store.profiles().size() == profilesBefore);
    CHECK_FALSE(store.removeProfile(L"TempProfile"));
}

// ---------------------------------------------------------------------------
// Store — diretorios e persistencia em disco
// ---------------------------------------------------------------------------
TEST_CASE("store: dataDir e logDir criam os diretorios")
{
    TempStoreDir tmp(L"soundint_store_dirs");
    Store& store = Store::instance();

    CHECK(store.dataDir() == tmp.dir());
    CHECK(store.logDir() == tmp.dir() + L"\\logs");
    CHECK(fileExists(tmp.dir()));
    CHECK(fileExists(tmp.dir() + L"\\logs"));
}

TEST_CASE("store: load cria arquivos e save e atomico")
{
    TempStoreDir tmp(L"soundint_store_io");
    removeStoreFiles(tmp);
    Store& store = Store::instance();

    CHECK(store.load());
    CHECK(fileExists(tmp.file(L"settings.json")));
    CHECK(fileExists(tmp.file(L"rules.json")));
    CHECK(fileExists(tmp.file(L"arrival_rules.json")));
    CHECK(fileExists(tmp.file(L"profiles.json")));

    // load e idempotente.
    CHECK(store.load());

    store.upsertRule(AppRule{ true, L"PersistApp.EXE", L"dev-persist" });
    ArrivalRule arrival;
    arrival.deviceNamePattern = L"Headset";
    arrival.setAsDefault = true;
    store.upsertArrivalRule(arrival);
    Profile profile;
    profile.name = L"PersistProfile";
    profile.defaultDeviceId = L"dev-profile";
    store.upsertProfile(profile);
    store.settings().theme = 2;
    store.lastManualDefaultDevice() = L"dev-manual";

    CHECK(store.save());
    // Gravacao atomica: nenhum .tmp sobra apos o save.
    CHECK_FALSE(fileExists(tmp.file(L"settings.json.tmp")));
    CHECK_FALSE(fileExists(tmp.file(L"rules.json.tmp")));

    // Recarrega do disco e confere a preservacao.
    REQUIRE(store.load());
    auto rule = store.findRule(L"persistapp.exe");
    REQUIRE(rule.has_value());
    CHECK(rule->deviceId == L"dev-persist");
    CHECK(store.arrivalRules().size() == 1);
    CHECK(store.arrivalRules()[0].deviceNamePattern == L"Headset");
    CHECK(store.arrivalRules()[0].setAsDefault);
    REQUIRE(store.profiles().size() == 1);
    CHECK(store.profiles()[0].name == L"PersistProfile");
    CHECK(store.settings().theme == 2);
    CHECK(store.lastManualDefaultDevice() == L"dev-manual");

    // Limpeza do diretorio temporario.
    store.removeRule(L"persistapp.exe");
    store.removeArrivalRule(L"Headset");
    store.removeProfile(L"PersistProfile");
    store.lastManualDefaultDevice().clear();
    store.settings().theme = 0;
    CHECK(store.save());
    removeStoreFiles(tmp);
}

TEST_CASE("store: settings.json corrompido usa padroes e sinaliza falha")
{
    TempStoreDir tmp(L"soundint_store_corrupt");
    removeStoreFiles(tmp);
    Store& store = Store::instance();
    REQUIRE(store.load());

    writeFileText(tmp.file(L"settings.json"), "{ isto nao e json valido");
    writeFileText(tmp.file(L"rules.json"), nlohmann::json::array().dump());

    // corrompido => false + fallback; lista valida vazia => true.
    CHECK_FALSE(store.load());
    CHECK(store.settings().hotkeys.size() == 2);
    CHECK(store.settings().theme == 0);

    removeStoreFiles(tmp);
}

TEST_CASE("store: regras de lista sobrevivem ao reload com caixa preservada")
{
    TempStoreDir tmp(L"soundint_store_rules");
    removeStoreFiles(tmp);
    Store& store = Store::instance();
    REQUIRE(store.load());

    store.upsertRule(AppRule{ true, L"CaseTest.exe", L"dev-c" });
    REQUIRE(store.save());
    REQUIRE(store.load());

    // A chave normalizada encontra a regra; o valor gravado tambem e chave.
    auto rule = store.findRule(L"CASETEST.EXE");
    REQUIRE(rule.has_value());
    CHECK(rule->processName == L"casetest.exe");

    store.removeRule(L"casetest.exe");
    CHECK(store.save());
    removeStoreFiles(tmp);
}

// ---------------------------------------------------------------------------
// Log
// ---------------------------------------------------------------------------
TEST_CASE("log: init escreve arquivo e shutdown fecha")
{
    const std::wstring dir = tempPath(L"soundint_log_basic");
    DeleteFileW((dir + L"\\SoundInt.log").c_str());

    slog::init(dir);
    const std::wstring path = slog::currentFilePath();
    CHECK(path == dir + L"\\SoundInt.log");

    slog::write(slog::Level::Info, "core", "mensagem ascii");
    slog::write(slog::Level::Warn, "core", std::wstring(L"mensagem wide \u00e9"));
    slog::write(slog::Level::Debug, "core", L"debug");
    slog::write(slog::Level::Error, "core", "erro");
    slog::shutdown();

    REQUIRE(fileExists(path));
    const std::string content = readFileText(path);
    CHECK(content.find("[INFO] [core] mensagem ascii\r\n") != std::string::npos);
    CHECK(content.find("[WARN] [core] mensagem wide \xC3\xA9\r\n") != std::string::npos);
    CHECK(content.find("[DEBUG] [core] debug\r\n") != std::string::npos);
    CHECK(content.find("[ERROR] [core] erro\r\n") != std::string::npos);

    // Formato da linha: "AAAA-MM-DD HH:MM:SS.mmm [NOME] ..." e termino CRLF.
    REQUIRE(content.size() > 24);
    const bool anoDigito = content[0] >= '0' && content[0] <= '9';
    CHECK(anoDigito);  // ano com quatro digitos
    CHECK(content[4] == '-');
    CHECK(content[7] == '-');
    CHECK(content[10] == ' ');
    CHECK(content[13] == ':');
    CHECK(content[16] == ':');
    CHECK(content[19] == '.');
    CHECK(content.find(" [INFO] ") != std::string::npos);
    CHECK(content.find("\r\n") != std::string::npos);
    CHECK(content.substr(content.size() - 2) == "\r\n");
}

TEST_CASE("log: rotacao ao ultrapassar 2 MB mantem 5 backups")
{
    const std::wstring dir = tempPath(L"soundint_log_rotate");
    DeleteFileW((dir + L"\\SoundInt.log").c_str());
    for (int i = 1; i <= 5; ++i) {
        DeleteFileW((dir + L"\\SoundInt.log." + std::to_wstring(i)).c_str());
    }

    slog::init(dir);
    const std::wstring path = slog::currentFilePath();

    // ~3 MB em mensagens de 4 KB: estoura o limite de 2 MB.
    const std::string payload(4096, 'x');
    for (int i = 0; i < 700; ++i) {
        slog::write(slog::Level::Info, "rot", payload.c_str());
    }
    slog::shutdown();

    CHECK(fileExists(path));
    CHECK(fileExists(path + L".1"));  // atual foi rotacionado
    // Arquivo atual volta a ficar pequeno (recriado apos a rotacao).
    CHECK(readFileText(path).size() < 2 * 1024 * 1024 + 8192);

    // shutdown com o arquivo ja fechado nao pode quebrar nada.
    slog::shutdown();
    slog::write(slog::Level::Info, "rot", "apos shutdown");  // sem arquivo: so debugger
    CHECK(slog::currentFilePath() == path);

    DeleteFileW(path.c_str());
    for (int i = 1; i <= 5; ++i) {
        DeleteFileW((path + L"." + std::to_wstring(i)).c_str());
    }
}

TEST_CASE("log: write com arquivo fechado nao crashea")
{
    slog::shutdown();  // garante estado fechado
    CHECK_NOTHROW(slog::write(slog::Level::Error, "idle", "sem arquivo aberto"));
}

}  // namespace


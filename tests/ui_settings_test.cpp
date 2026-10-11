// ============================================================================
// Testes do U3 (Track K) - tabelas i18n, normalizacao de regras, montagem do
// registro de inicializacao, status transitorio com desfazer e mapeamento
// das escolhas de saida (sem janela, sem gravar no registro).
// ============================================================================
#include "doctest.h"

#include "ui/i18n.h"
#include "ui/windows/settings/settings_undo.h"
#include "ui/windows/settings/settings_window.h"

#include "soundint/version.h"

#include <string>
#include <vector>

namespace {

namespace i18n = soundint::ui;
namespace settings = soundint::ui::settings;

// Chaves obrigatorias dos demais tracks (outros agents ja as usam).
const wchar_t* const kRequiredKeys[] = {
    L"common.apply",
    L"common.cancel",
    L"common.close",
    L"common.undo",
    L"modal.newDevice.title",
    L"modal.newDevice.primary",
    L"modal.newDevice.secondary",
    L"modal.newDevice.remember",
    L"modal.newApp.title",
    L"modal.newApp.outputLabel",
    L"modal.newApp.systemDefault",
    L"modal.newApp.remember",
    L"mixer.title",
    L"mixer.outputs",
    L"mixer.apps",
    L"mixer.systemSounds",
    L"mixer.empty",
    L"mixer.defaultOutputLabel",
    L"mixer.volume",
    L"mixer.mute",
};

// Chaves da janela de settings (garante a integracao Wave 3).
const wchar_t* const kSettingsKeys[] = {
    L"settings.title",
    L"settings.nav.general",
    L"settings.nav.rules",
    L"settings.nav.profiles",
    L"settings.nav.hotkeys",
    L"settings.nav.updates",
    L"settings.nav.about",
    L"settings.general.title",
    L"settings.rules.title",
    L"settings.rules.processLabel",
    L"settings.rules.outputLabel",
    L"settings.rules.placeholder",
    L"settings.rules.add",
    L"settings.rules.systemDefault",
    L"settings.rules.remove",
    L"settings.rules.enabled",
    L"settings.rules.active",
    L"settings.rules.hint",
    L"settings.rules.invalid",
    L"settings.rules.added",
    L"settings.rules.updated",
    L"settings.rules.removed",
    L"settings.rules.restored",
    L"settings.profiles.title",
    L"settings.profiles.nameLabel",
    L"settings.profiles.placeholder",
    L"settings.profiles.create",
    L"settings.profiles.hint",
    L"settings.profiles.remove",
    L"settings.profiles.selectHint",
    L"settings.profiles.overrides",
    L"settings.profiles.overrideProcess",
    L"settings.profiles.overrideDevice",
    L"settings.profiles.overrideProcessPlaceholder",
    L"settings.profiles.addOverride",
    L"settings.profiles.removeOverride",
    L"settings.profiles.created",
    L"settings.profiles.exists",
    L"settings.profiles.removed",
    L"settings.profiles.restored",
    L"settings.profiles.applied",
    L"settings.profiles.routeSaved",
    L"settings.profiles.routeRemoved",
    L"settings.profiles.routeRestored",
    L"settings.profiles.invalid",
    L"settings.profiles.invalidOverride",
    L"settings.profiles.noRouter",
    L"settings.hotkeys.title",
    L"settings.hotkeys.record",
    L"settings.hotkeys.recording",
    L"settings.hotkeys.none",
    L"settings.hotkeys.hint",
    L"settings.hotkeys.active",
    L"settings.hotkeys.unassigned",
    L"settings.hotkeys.cancelled",
    L"settings.hotkeys.mixer",
    L"settings.hotkeys.cycleOutput",
    L"settings.hotkeys.profile",
    L"settings.hotkeys.empty",
    L"settings.updates.title",
    L"settings.updates.autoCheck",
    L"settings.updates.betaChannel",
    L"settings.updates.checkNow",
    L"settings.updates.checking",
    L"settings.updates.download",
    L"settings.updates.available",
    L"settings.updates.upToDate",
    L"settings.updates.noService",
    L"settings.about.title",
    L"settings.about.tagline",
    L"settings.about.version",
    L"settings.about.license",
    L"settings.about.repo",
    L"settings.about.reportIssue",
    L"settings.about.openLogs",
    L"settings.about.credits",
    L"settings.about.creditsEarTrumpet",
};

// Chaves de secao criadas para a pagina Geral (consumidas pela U2).
const wchar_t* const kGeneralSectionKeys[] = {
    L"settings.general.section.notifications",
    L"settings.general.section.behavior",
    L"settings.general.section.startup",
    L"settings.general.section.appearance",
    L"settings.general.startupFailed",
};

// Restaura o idioma "auto" ao fim de cada TEST_CASE.
class LanguageGuard {
public:
    ~LanguageGuard()
    {
        i18n::setLanguage(L"auto");
    }
};

// Relogio falso injetavel no PageStatus (captureless -> ponteiro de funcao).
ULONGLONG g_fakeNow = 0;

bool translated(const wchar_t* key)
{
    return std::wstring(i18n::tr(key)) != std::wstring(key) && i18n::tr(key)[0] != L'\0';
}

// Busca case-insensitive (nav "Shortcuts" vs titulo "Global shortcuts").
bool containsIgnoreCase(const std::wstring& haystack, const std::wstring& needle)
{
    if (needle.empty()) {
        return true;
    }
    auto lower = [](std::wstring text) {
        for (wchar_t& c : text) {
            if (c >= L'A' && c <= L'Z') {
                c = static_cast<wchar_t>(c - L'A' + L'a');
            }
        }
        return text;
    };
    const std::wstring hay = lower(haystack);
    const std::wstring nee = lower(needle);
    return hay.find(nee) != std::wstring::npos;
}

std::wstring widenAscii(const char* text)
{
    std::wstring out;
    for (const char* c = text; c != nullptr && *c != '\0'; ++c) {
        out.push_back(static_cast<wchar_t>(*c));
    }
    return out;
}

}  // namespace

TEST_CASE("i18n: chaves obrigatorias traduzidas em pt-BR e en")
{
    LanguageGuard guard;

    i18n::setLanguage(L"pt-BR");
    for (const wchar_t* key : kRequiredKeys) {
        CHECK(translated(key));
    }

    i18n::setLanguage(L"en");
    for (const wchar_t* key : kRequiredKeys) {
        CHECK(translated(key));
    }
}

TEST_CASE("i18n: chaves de settings traduzidas em pt-BR e en")
{
    LanguageGuard guard;

    i18n::setLanguage(L"pt-BR");
    for (const wchar_t* key : kSettingsKeys) {
        CHECK(translated(key));
    }

    i18n::setLanguage(L"en");
    for (const wchar_t* key : kSettingsKeys) {
        CHECK(translated(key));
    }
}

TEST_CASE("i18n: chaves das secoes da pagina Geral (consumo da U2)")
{
    LanguageGuard guard;

    i18n::setLanguage(L"pt-BR");
    for (const wchar_t* key : kGeneralSectionKeys) {
        CHECK(translated(key));
    }

    i18n::setLanguage(L"en");
    for (const wchar_t* key : kGeneralSectionKeys) {
        CHECK(translated(key));
    }
}

TEST_CASE("i18n: tabelas pt-BR e en diferem de verdade")
{
    LanguageGuard guard;

    i18n::setLanguage(L"pt-BR");
    const std::wstring ptAbout = i18n::tr(L"settings.nav.about");
    const std::wstring ptRules = i18n::tr(L"settings.rules.empty");

    i18n::setLanguage(L"en");
    const std::wstring enAbout = i18n::tr(L"settings.nav.about");
    const std::wstring enRules = i18n::tr(L"settings.rules.empty");

    CHECK(ptAbout != enAbout);
    CHECK(ptRules != enRules);
}

TEST_CASE("i18n: chave desconhecida devolve a propria chave")
{
    LanguageGuard guard;

    i18n::setLanguage(L"pt-BR");
    CHECK(std::wstring(i18n::tr(L"nao.existe.esta.chave")) == L"nao.existe.esta.chave");

    i18n::setLanguage(L"en");
    CHECK(std::wstring(i18n::tr(L"nao.existe.esta.chave")) == L"nao.existe.esta.chave");

    CHECK(std::wstring(i18n::tr(nullptr)) == L"");
}

TEST_CASE("i18n: setLanguage/currentLanguage com codigos validos e invalidos")
{
    LanguageGuard guard;

    i18n::setLanguage(L"pt-BR");
    CHECK(std::wstring(i18n::currentLanguage()) == L"pt-BR");

    i18n::setLanguage(L"PT-br");   // case-insensitive -> canonico
    CHECK(std::wstring(i18n::currentLanguage()) == L"pt-BR");

    i18n::setLanguage(L"en");
    CHECK(std::wstring(i18n::currentLanguage()) == L"en");

    i18n::setLanguage(L"EN");
    CHECK(std::wstring(i18n::currentLanguage()) == L"en");

    i18n::setLanguage(L"auto");
    CHECK(std::wstring(i18n::currentLanguage()) == L"auto");

    i18n::setLanguage(L"xx-YY");   // invalido -> auto
    CHECK(std::wstring(i18n::currentLanguage()) == L"auto");

    i18n::setLanguage(L"");        // invalido -> auto
    CHECK(std::wstring(i18n::currentLanguage()) == L"auto");

    i18n::setLanguage(nullptr);    // invalido -> auto
    CHECK(std::wstring(i18n::currentLanguage()) == L"auto");
}

TEST_CASE("i18n: versao instalada aparece na pagina Sobre")
{
    LanguageGuard guard;

    i18n::setLanguage(L"en");
    const std::wstring text = i18n::tr(L"settings.about.version");
    CHECK(text.find(widenAscii(SOUNDINT_VERSION_STRING)) != std::wstring::npos);

    // P2: Updates nao duplica a versao instalada — a chave foi removida e o
    // tr() devolve a propria chave (fallback de chave inexistente).
    CHECK(std::wstring(i18n::tr(L"settings.updates.current")) ==
          L"settings.updates.current");
    // O campo de deviceId cru virou Dropdown: placeholder ficou orfao e foi
    // removido junto.
    CHECK(std::wstring(i18n::tr(L"settings.profiles.overrideDevicePlaceholder")) ==
          L"settings.profiles.overrideDevicePlaceholder");
}

TEST_CASE("i18n: navegacao e titulos usam o mesmo vocabulario")
{
    LanguageGuard guard;

    struct Pair {
        const wchar_t* nav;
        const wchar_t* title;
    };
    const Pair pairs[] = {
        {L"settings.nav.general", L"settings.general.title"},
        {L"settings.nav.rules", L"settings.rules.title"},
        {L"settings.nav.profiles", L"settings.profiles.title"},
        {L"settings.nav.hotkeys", L"settings.hotkeys.title"},
        {L"settings.nav.updates", L"settings.updates.title"},
        {L"settings.nav.about", L"settings.about.title"},
    };

    for (const wchar_t* lang : {L"pt-BR", L"en"}) {
        i18n::setLanguage(lang);
        for (const Pair& pair : pairs) {
            const std::wstring nav = i18n::tr(pair.nav);
            const std::wstring title = i18n::tr(pair.title);
            CHECK(containsIgnoreCase(title, nav));
        }
    }
}

TEST_CASE("i18n: UI de perfis nunca expoe 'override' nem deviceId cru")
{
    LanguageGuard guard;

    const wchar_t* const keys[] = {
        L"settings.profiles.overrides",
        L"settings.profiles.overrideProcess",
        L"settings.profiles.overrideDevice",
        L"settings.profiles.addOverride",
        L"settings.profiles.removeOverride",
    };

    for (const wchar_t* lang : {L"pt-BR", L"en"}) {
        i18n::setLanguage(lang);
        for (const wchar_t* key : keys) {
            const std::wstring value = i18n::tr(key);
            CHECK(value.find(L"override") == std::wstring::npos);
            CHECK(value.find(L"Override") == std::wstring::npos);
            CHECK(value.find(L"deviceId") == std::wstring::npos);
        }
    }
}

TEST_CASE("undo: status transitorio expira e executa o desfazer")
{
    using settings::PageStatus;
    using settings::StatusKind;

    g_fakeNow = 1000;
    PageStatus::setClockForTests([]() -> ULONGLONG { return g_fakeNow; });

    bool undone = false;
    PageStatus status;
    status.show(StatusKind::Info, L"Removido", PageStatus::kUndoTimeoutMs,
                [&undone]() { undone = true; });
    CHECK(status.active());
    CHECK(status.hasUndo());
    CHECK(status.text() == L"Removido");

    g_fakeNow += 4000;
    CHECK(status.active());

    g_fakeNow += 1000;   // prazo do desfazer vencido: some sozinho
    CHECK(!status.active());
    CHECK(!status.hasUndo());

    // Novo status + desfazer imediato restaura e limpa.
    status.show(StatusKind::Danger, L"Erro", 4000u, [&undone]() { undone = true; });
    status.runUndo();
    CHECK(undone);
    CHECK(!status.active());
    CHECK(status.text().empty());

    // Sem desfazer configurado, runUndo e inofensivo.
    status.show(StatusKind::Success, L"Salvo");
    CHECK(!status.hasUndo());
    status.runUndo();
    CHECK(status.active());
    status.clear();
    CHECK(!status.active());

    PageStatus::setClockForTests(nullptr);
}

TEST_CASE("undo: OutputChoices mapeia deviceId e indice do Dropdown")
{
    settings::OutputChoices choices;

    soundint::DeviceInfo phones;
    phones.id = L"{phones-guid}";
    phones.friendlyName = L"Phones";
    soundint::DeviceInfo monitor;
    monitor.id = L"{monitor-guid}";
    monitor.friendlyName = L"Monitor";

    choices.refresh({phones, monitor}, L"System default");
    REQUIRE(choices.items().size() == 3);
    CHECK(choices.items()[0] == L"System default");
    CHECK(choices.items()[1] == L"Phones");
    CHECK(choices.items()[2] == L"Monitor");

    // Item 0 = padrao do sistema (deviceId vazio).
    CHECK(choices.indexForId(L"") == 0);
    CHECK(choices.idForIndex(0) == L"");

    // Mapeamento id <-> indice (case-insensitive no id do endpoint).
    CHECK(choices.indexForId(L"{phones-guid}") == 1);
    CHECK(choices.indexForId(L"{PHONES-GUID}") == 1);
    CHECK(choices.idForIndex(1) == L"{phones-guid}");
    CHECK(choices.idForIndex(2) == L"{monitor-guid}");

    // Desconhecido/fora de faixa cai no padrao do sistema.
    CHECK(choices.indexForId(L"{missing}") == 0);
    CHECK(choices.idForIndex(99) == L"");

    // Nome amigavel com fallbacks (vazio => padrao; offline => id cru).
    CHECK(choices.friendlyName(L"{monitor-guid}", L"System default") == L"Monitor");
    CHECK(choices.friendlyName(L"", L"System default") == L"System default");
    CHECK(choices.friendlyName(L"{missing}", L"System default") == L"{missing}");
}

TEST_CASE("settings: normalizacao do processName das regras")
{
    CHECK(settings::normalizeProcessName(L"  Spotify.EXE ") == L"spotify.exe");
    CHECK(settings::normalizeProcessName(L"foo.exe") == L"foo.exe");
    CHECK(settings::normalizeProcessName(L"FOO.EXE") == L"foo.exe");
    CHECK(settings::normalizeProcessName(L"") == L"");
    CHECK(settings::normalizeProcessName(L"   \t\r\n") == L"");
    // So trim + minusculas: nao inventa sufixo .exe.
    CHECK(settings::normalizeProcessName(L"spotify") == L"spotify");
}

TEST_CASE("settings: startupCommandLine monta o valor entre aspas")
{
    const std::wstring command = settings::startupCommandLine(L"C:\\Apps\\SoundInt.exe");
    CHECK(command == L"\"C:\\Apps\\SoundInt.exe\"");

    const std::wstring empty = settings::startupCommandLine(L"");
    CHECK(empty == L"\"\"");
}

TEST_CASE("settings: startupValue usa o caminho do exe (sem registro)")
{
    const std::wstring value = settings::startupValue();
    REQUIRE(value.size() > 3);
    CHECK(value.front() == L'"');
    CHECK(value.back() == L'"');

    // Contem ".exe" sem depender de maiusculas/minusculas do caminho.
    std::wstring lower;
    for (wchar_t c : value) {
        if (c >= L'A' && c <= L'Z') {
            lower.push_back(static_cast<wchar_t>(c - L'A' + L'a'));
        } else {
            lower.push_back(c);
        }
    }
    CHECK(lower.find(L".exe") != std::wstring::npos);
}

// ============================================================================
// Testes do Track K - tabelas i18n, normalizacao de regras e montagem do
// registro de inicializacao (sem janela, sem gravar no registro).
// ============================================================================
#include "doctest.h"

#include "ui/i18n.h"
#include "ui/windows/settings/settings_window.h"

#include "soundint/version.h"

#include <string>

namespace {

namespace i18n = soundint::ui;
namespace settings = soundint::ui::settings;

// Chaves obrigatorias dos demais tracks (outros agents ja as usam).
const wchar_t* const kRequiredKeys[] = {
    L"common.apply",
    L"common.cancel",
    L"common.close",
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
    L"settings.profiles.title",
    L"settings.hotkeys.title",
    L"settings.updates.title",
    L"settings.about.title",
    L"settings.updates.checkNow",
    L"settings.hotkeys.record",
    L"settings.rules.add",
};

// Restaura o idioma "auto" ao fim de cada TEST_CASE.
class LanguageGuard {
public:
    ~LanguageGuard()
    {
        i18n::setLanguage(L"auto");
    }
};

bool translated(const wchar_t* key)
{
    return std::wstring(i18n::tr(key)) != std::wstring(key) && i18n::tr(key)[0] != L'\0';
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

TEST_CASE("i18n: versao atual composta na chave updates.current")
{
    LanguageGuard guard;

    i18n::setLanguage(L"en");
    const std::wstring text = i18n::tr(L"settings.updates.current");
    CHECK(text.find(widenAscii(SOUNDINT_VERSION_STRING)) != std::wstring::npos);
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

// ============================================================================
// Testes da politica pura do app-shell (Track H) — sem Win32.
// ============================================================================
#include "doctest.h"

#include "app/policy.h"

using soundint::AppRule;
using soundint::ArrivalRule;
using soundint::SessionInfo;
using soundint::policy::CooldownTracker;
using soundint::policy::Decision;
using soundint::policy::Debouncer;
using soundint::policy::TimeMs;

namespace {

SessionInfo makeSession(uint32_t pid, const std::wstring& name, bool systemSounds = false)
{
    SessionInfo session;
    session.pid = pid;
    session.processName = name;
    session.systemSounds = systemSounds;
    session.active = true;
    session.instanceId = name + L"#1";
    return session;
}

AppRule makeRule(const std::wstring& name, bool enabled = true,
                 const std::wstring& deviceId = L"")
{
    AppRule rule;
    rule.processName = name;
    rule.enabled = enabled;
    rule.deviceId = deviceId;
    return rule;
}

}  // namespace

TEST_CASE("policy: iEqual e iContains ignoram maiusculas")
{
    CHECK(soundint::policy::iEqual(L"spotify.exe", L"Spotify.exe"));
    CHECK(soundint::policy::iEqual(L"", L""));
    CHECK_FALSE(soundint::policy::iEqual(L"spotify.exe", L"spotify"));
    CHECK_FALSE(soundint::policy::iEqual(L"foo.exe", L"bar.exe"));

    CHECK(soundint::policy::iContains(L"Artileria Bluetooth Headset", L"bluetooth"));
    CHECK(soundint::policy::iContains(L"USB AUDIO", L"usb"));
    CHECK_FALSE(soundint::policy::iContains(L"USB AUDIO", L"hdmi"));
    // Padrao vazio nao casa (regra mal preenchida nao pode disparar nada).
    CHECK_FALSE(soundint::policy::iContains(L"qualquer nome", L""));
    // Padrao maior que o nome.
    CHECK_FALSE(soundint::policy::iContains(L"abc", L"abcdef"));
}

TEST_CASE("policy: isSystemSounds cobre pid 0 e a flag")
{
    CHECK(soundint::policy::isSystemSounds(makeSession(0, L"system")));
    CHECK(soundint::policy::isSystemSounds(makeSession(1234, L"ding.wav", true)));
    CHECK_FALSE(soundint::policy::isSystemSounds(makeSession(1234, L"spotify.exe")));
}

TEST_CASE("policy: hasRule exige regra existente e habilitada")
{
    CHECK_FALSE(soundint::policy::hasRule(std::nullopt));
    CHECK_FALSE(soundint::policy::hasRule(makeRule(L"spotify.exe", /*enabled=*/false)));
    CHECK(soundint::policy::hasRule(makeRule(L"spotify.exe")));
}

TEST_CASE("policy: cooldownRemaining devolve ms e zera fora da janela")
{
    CHECK(soundint::policy::cooldownRemaining(1000, 0, 500) == 0);       // nunca usou
    CHECK(soundint::policy::cooldownRemaining(1000, 1000, 500) == 500);  // comecou agora
    CHECK(soundint::policy::cooldownRemaining(1200, 1000, 500) == 300);
    CHECK(soundint::policy::cooldownRemaining(1500, 1000, 500) == 0);    // venceu
    CHECK(soundint::policy::cooldownRemaining(9000, 1000, 500) == 0);
    CHECK(soundint::policy::cooldownRemaining(1000, 1000, 0) == 0);      // sem cooldown
}

TEST_CASE("policy: decideSession - sons do sistema sempre ignoram")
{
    const SessionInfo systemSounds = makeSession(0, L"system");
    const Decision d = soundint::policy::decideSession(
        systemSounds, makeRule(L"system"), true, 10'000, 0, 60'000);
    CHECK(d == Decision::Ignore);
}

TEST_CASE("policy: decideSession - regra habilitada aplica silenciosamente")
{
    const SessionInfo session = makeSession(4242, L"spotify.exe");

    CHECK(soundint::policy::decideSession(session, makeRule(L"spotify.exe"), true, 10'000, 0,
                                          60'000) == Decision::ApplyRuleSilently);
    // Regra vale mesmo com popup desligado e com cooldown cheio.
    CHECK(soundint::policy::decideSession(session, makeRule(L"spotify.exe"), false, 10'000,
                                          9'000, 60'000) ==
          Decision::ApplyRuleSilently);
}

TEST_CASE("policy: decideSession - sem regra decide pelo popup/cooldown")
{
    const SessionInfo session = makeSession(4242, L"spotify.exe");
    const std::optional<AppRule> none = std::nullopt;

    // Regra existente mas desabilitada nao vale como regra.
    CHECK(soundint::policy::decideSession(session, makeRule(L"spotify.exe", false), true,
                                          10'000, 0, 60'000) == Decision::AskPopup);

    // Popup desligado -> nada a perguntar.
    CHECK(soundint::policy::decideSession(session, none, false, 10'000, 0, 60'000) ==
          Decision::Ignore);

    // Livre -> pergunta.
    CHECK(soundint::policy::decideSession(session, none, true, 10'000, 0, 60'000) ==
          Decision::AskPopup);

    // Dentro do cooldown (comecou em 50s, janela de 60s) -> suprimido.
    CHECK(soundint::policy::decideSession(session, none, true, 100'000, 50'000, 60'000) ==
          Decision::SuppressCooldown);

    // Cooldown vencido -> pergunta de novo.
    CHECK(soundint::policy::decideSession(session, none, true, 120'000, 50'000, 60'000) ==
          Decision::AskPopup);
}

TEST_CASE("policy: decideDevice - regra de chegada casou silencia o popup")
{
    CHECK(soundint::policy::decideDevice(/*arrivalMatched=*/true, /*popupSetting=*/true,
                                         10'000, 0, 300'000) == Decision::Ignore);
    CHECK(soundint::policy::decideDevice(false, false, 10'000, 0, 300'000) ==
          Decision::Ignore);
    CHECK(soundint::policy::decideDevice(false, true, 10'000, 0, 300'000) ==
          Decision::AskPopup);
    CHECK(soundint::policy::decideDevice(false, true, 399'999, 100'000, 300'000) ==
          Decision::SuppressCooldown);
    CHECK(soundint::policy::decideDevice(false, true, 400'000, 100'000, 300'000) ==
          Decision::AskPopup);
}

TEST_CASE("policy: matchArrival casa por substring case-insensitive")
{
    std::vector<ArrivalRule> rules;
    CHECK(soundint::policy::matchArrival(rules, L"JBL Flip 5") == nullptr);

    ArrivalRule bluetooth;
    bluetooth.deviceNamePattern = L"Bluetooth";
    bluetooth.setAsDefault = true;
    rules.push_back(bluetooth);

    ArrivalRule disabled;
    disabled.enabled = false;
    disabled.deviceNamePattern = L"USB";
    rules.push_back(disabled);

    ArrivalRule empty;
    empty.deviceNamePattern = L"";
    rules.push_back(empty);

    const ArrivalRule* match = soundint::policy::matchArrival(rules, L"Artileria BLUETOOTH");
    REQUIRE(match != nullptr);
    CHECK(match->deviceNamePattern == L"Bluetooth");

    // Regra desabilitada e padrao vazio nao casam.
    CHECK(soundint::policy::matchArrival(rules, L"Cabo USB Audio") == nullptr);
    CHECK(soundint::policy::matchArrival(rules, L"") == nullptr);
}

TEST_CASE("policy: Debouncer coalesce por chave com relogio injetado")
{
    Debouncer debouncer(800);
    CHECK(debouncer.interval() == 800);

    // Primeiro evento agenda now + 800.
    CHECK(debouncer.poke(L"dev-1", 1'000) == 1'800);
    CHECK(debouncer.remaining(L"dev-1", 1'000) == 800);
    CHECK(debouncer.remaining(L"dev-1", 1'500) == 300);
    CHECK_FALSE(debouncer.consumeDue(L"dev-1", 1'799));
    CHECK(debouncer.has(L"dev-1"));

    // Evento repetido reagenda o deadline (coalesce).
    CHECK(debouncer.poke(L"dev-1", 1'700) == 2'500);
    CHECK_FALSE(debouncer.consumeDue(L"dev-1", 2'499));

    // Chaves sao independentes.
    CHECK(debouncer.poke(L"dev-2", 2'000) == 2'800);
    CHECK(debouncer.consumeDue(L"dev-1", 2'500));
    CHECK_FALSE(debouncer.has(L"dev-1"));
    // Consome uma unica vez.
    CHECK_FALSE(debouncer.consumeDue(L"dev-1", 3'000));
    CHECK(debouncer.remaining(L"dev-2", 2'600) == 200);

    // erase/clear limpam a janela.
    debouncer.erase(L"dev-2");
    CHECK_FALSE(debouncer.has(L"dev-2"));
    debouncer.poke(L"dev-3", 3'000);
    debouncer.clear();
    CHECK_FALSE(debouncer.has(L"dev-3"));
    CHECK_FALSE(debouncer.consumeDue(L"dev-3", 99'000));
}

TEST_CASE("policy: Debouncer com intervalo zerado dispara no mesmo instante")
{
    Debouncer debouncer(0);
    CHECK(debouncer.poke(L"k", 500) == 500);
    CHECK(debouncer.remaining(L"k", 500) == 0);
    CHECK(debouncer.consumeDue(L"k", 500));
}

TEST_CASE("policy: CooldownTracker libera apos a janela e isola chaves")
{
    CooldownTracker cooldown(10'000);
    CHECK(cooldown.cooldown() == 10'000);
    CHECK(cooldown.last(L"app.exe") == 0);
    CHECK(cooldown.remaining(L"app.exe", 1'000) == 0);

    CHECK(cooldown.tryAcquire(L"app.exe", 1'000));
    CHECK_FALSE(cooldown.tryAcquire(L"app.exe", 9'999));
    CHECK(cooldown.remaining(L"app.exe", 9'999) == 1001);
    CHECK(cooldown.last(L"app.exe") == 1'000);

    // Outra chave nao e afetada.
    CHECK(cooldown.tryAcquire(L"outro.exe", 2'000));

    // Venceu o cooldown -> pode de novo.
    CHECK(cooldown.remaining(L"app.exe", 11'000) == 0);
    CHECK(cooldown.tryAcquire(L"app.exe", 11'000));
    CHECK(cooldown.last(L"app.exe") == 11'000);

    cooldown.erase(L"app.exe");
    CHECK(cooldown.last(L"app.exe") == 0);
    cooldown.mark(L"x.exe", 42);
    CHECK(cooldown.last(L"x.exe") == 42);
    cooldown.clear();
    CHECK(cooldown.last(L"outro.exe") == 0);
}

TEST_CASE("policy: now() e monotono e positivo")
{
    const TimeMs a = soundint::policy::now();
    const TimeMs b = soundint::policy::now();
    CHECK(a > 0);
    CHECK(b >= a);
}

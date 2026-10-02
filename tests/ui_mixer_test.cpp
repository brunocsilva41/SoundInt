// ============================================================================
// Testes do flyout do mixer (Track J) — apenas logica pura, sem HWND.
// ============================================================================
#include "doctest.h"

#include "ui/design_tokens.h"
#include "ui/i18n.h"
#include "ui/windows/mixer.h"

#include <string>
#include <vector>

using namespace soundint;
using namespace soundint::ui;
using namespace soundint::ui::mixer;

namespace {

DeviceInfo makeDevice(const wchar_t* id, const wchar_t* name)
{
    DeviceInfo device;
    device.id = id;
    device.friendlyName = name;
    device.flow = Flow::Render;
    device.active = true;
    return device;
}

SessionInfo makeSession(const wchar_t* instanceId, const wchar_t* name, bool active,
                        bool systemSounds = false, const wchar_t* deviceId = L"")
{
    SessionInfo session;
    session.instanceId = instanceId;
    session.displayName = name;
    session.processName = name;
    session.active = active;
    session.systemSounds = systemSounds;
    session.deviceId = deviceId;
    session.volume = 1.0f;
    return session;
}

bool contains(const RECT& rect, int x, int y)
{
    return x >= rect.left && x < rect.right && y >= rect.top && y < rect.bottom;
}

}  // namespace

// ---------------------------------------------------------------------------
// computeMixerRect — posicao ancorada acima-esquerda com clamp nas 4 bordas
// ---------------------------------------------------------------------------
TEST_CASE("computeMixerRect coloca o flyout acima e a esquerda da ancora")
{
    RECT work{0, 0, 1920, 1040};
    POINT anchor{800, 600};
    const RECT rect = computeMixerRect(anchor, {360.f, 300.f}, work);

    CHECK(rect.left == 800 - 360 - 8);
    CHECK(rect.top == 600 - 300 - 12);
    CHECK(rect.right - rect.left == 360);
    CHECK(rect.bottom - rect.top == 300);
}

TEST_CASE("computeMixerRect mantem o flyout 100% dentro da work area")
{
    const Size size{360.f, 300.f};

    SUBCASE("borda esquerda")
    {
        RECT work{0, 0, 1920, 1040};
        const RECT rect = computeMixerRect({100, 600}, size, work);
        CHECK(rect.left == work.left);
        CHECK(rect.right - rect.left == 360);
        CHECK(contains(rect, rect.left, rect.top));
    }

    SUBCASE("borda superior")
    {
        RECT work{0, 0, 1920, 1040};
        const RECT rect = computeMixerRect({800, 100}, size, work);
        CHECK(rect.top == work.top);
        CHECK(rect.bottom - rect.top == 300);
    }

    SUBCASE("borda direita")
    {
        RECT work{0, 0, 400, 1040};
        const RECT rect = computeMixerRect({450, 600}, size, work);
        CHECK(rect.right <= work.right);
        CHECK(rect.right - rect.left == 360);
    }

    SUBCASE("borda inferior")
    {
        RECT work{0, 0, 1920, 1040};
        const RECT rect = computeMixerRect({800, 1100}, size, work);
        CHECK(rect.bottom <= work.bottom);
        CHECK(rect.bottom - rect.top == 300);
    }

    SUBCASE("janela maior que a work area cola na origem")
    {
        RECT work{0, 0, 200, 200};
        const RECT rect = computeMixerRect({150, 150}, {360.f, 300.f}, work);
        CHECK(rect.left == work.left);
        CHECK(rect.top == work.top);
    }
}

// ---------------------------------------------------------------------------
// contentHeightFor / planLayout — altura com clamp de 70% e scroll
// ---------------------------------------------------------------------------
TEST_CASE("contentHeightFor soma secoes, linhas, divisor e gaps")
{
    ContentSpec spec;
    spec.outputRows = 1;
    spec.appRows = 0;
    spec.systemRow = false;

    // secao 20 + linha 40 + divisor 1 + secao 20 + vazio 40 + 4 gaps de 4
    const float expected = 20.f + 40.f + 1.f + 20.f + 40.f + 4.f * 4.f;
    CHECK(contentHeightFor(spec) == doctest::Approx(expected));

    ContentSpec withSystem = spec;
    withSystem.appRows = 2;
    withSystem.systemRow = true;
    const float expected2 = 20.f + 40.f + 1.f + 20.f + 80.f + 80.f + 80.f + 6.f * 4.f;
    CHECK(contentHeightFor(withSystem) == doctest::Approx(expected2));

    // secao "Apps abertos": + secao 20 + 2 linhas 80 + 3 gaps
    ContentSpec withIdle = withSystem;
    withIdle.idleAppRows = 2;
    const float expected3 = expected2 + 20.f + 80.f + 80.f + 3.f * 4.f;
    CHECK(contentHeightFor(withIdle) == doctest::Approx(expected3));
}

TEST_CASE("planLayout nao rola quando o conteudo cabe na work area")
{
    ContentSpec spec;
    spec.outputRows = 1;

    const LayoutPlan plan = planLayout(spec, 800.f);
    CHECK(plan.contentHeight == doctest::Approx(137.f));
    CHECK(plan.windowHeight == doctest::Approx(plan.contentHeight + 68.f));
    CHECK(plan.viewport == doctest::Approx(plan.contentHeight));
    CHECK(plan.maxScroll == doctest::Approx(0.f));
}

TEST_CASE("planLayout limita a janela em 70% e calcula o scroll maximo")
{
    ContentSpec spec;
    spec.outputRows = 3;
    spec.appRows = 6;
    spec.systemRow = true;

    const float content = contentHeightFor(spec);
    const LayoutPlan plan = planLayout(spec, 700.f);

    CHECK(content > 0.f);
    CHECK(plan.windowHeight == doctest::Approx(700.f * 0.7f));
    CHECK(plan.panelTop == doctest::Approx(56.f));
    CHECK(plan.viewport == doctest::Approx(plan.windowHeight - 56.f - 12.f));
    CHECK(plan.maxScroll == doctest::Approx(content - plan.viewport));
    CHECK(plan.contentHeight + 68.f > plan.windowHeight);   // conteudo excede
}

TEST_CASE("planLayout mantem um minimo de altura em work areas minusculas")
{
    ContentSpec spec;
    spec.outputRows = 1;
    const LayoutPlan plan = planLayout(spec, 40.f);
    CHECK(plan.windowHeight >= 96.f);
    CHECK(plan.viewport >= 0.f);
    CHECK(plan.maxScroll >= 0.f);
}

TEST_CASE("planLayout soma a altura extra do dropdown aberto")
{
    ContentSpec closed;
    closed.outputRows = 2;
    closed.appRows = 1;

    ContentSpec open = closed;
    open.dropdownItems = 3;   // padrao do sistema + 2 saidas

    const float extra = contentHeightFor(open) - contentHeightFor(closed);
    CHECK(extra == doctest::Approx(8.f + 32.f * 4.f));
}

// ---------------------------------------------------------------------------
// orderSessions / planSessions — systemSounds separado no fim, ativos primeiro
// ---------------------------------------------------------------------------
TEST_CASE("orderSessions coloca ativos primeiro e systemSounds por ultimo")
{
    std::vector<SessionInfo> sessions;
    sessions.push_back(makeSession(L"a", L"ativo1.exe", true));
    sessions.push_back(makeSession(L"s", L"system.exe", true, true));
    sessions.push_back(makeSession(L"i", L"inativo.exe", false));
    sessions.push_back(makeSession(L"b", L"ativo2.exe", true));
    sessions.push_back(makeSession(L"s2", L"system2.exe", true, true));

    const std::vector<SessionInfo> ordered = orderSessions(sessions);
    REQUIRE(ordered.size() == 5);

    CHECK(ordered[0].instanceId == L"a");
    CHECK(ordered[1].instanceId == L"b");
    CHECK(ordered[2].instanceId == L"i");       // inativo apos os ativos
    CHECK(ordered[3].instanceId == L"s");       // sistema no fim
    CHECK(ordered[4].instanceId == L"s2");      // segunda sessao de sistema
}

TEST_CASE("planSessions separa apps ativos, parados e systemSounds")
{
    std::vector<SessionInfo> sessions;
    sessions.push_back(makeSession(L"a", L"ativo.exe", true));
    sessions.push_back(makeSession(L"s", L"system.exe", true, true));
    sessions.push_back(makeSession(L"i", L"inativo.exe", false));

    const SessionPlan plan = planSessions(sessions);
    REQUIRE(plan.apps.size() == 1);
    CHECK(plan.apps[0].instanceId == L"a");
    REQUIRE(plan.idleApps.size() == 1);      // listagem "Apps abertos"
    CHECK(plan.idleApps[0].instanceId == L"i");
    CHECK(plan.idleApps[0].processName == L"inativo.exe");
    CHECK(plan.systemSounds);
    CHECK(plan.system.instanceId == L"s");

    SUBCASE("apenas systemSounds => listas de apps vazias (estado vazio)")
    {
        std::vector<SessionInfo> onlySystem;
        onlySystem.push_back(makeSession(L"s", L"system.exe", false, true));
        const SessionPlan sysPlan = planSessions(onlySystem);
        CHECK(sysPlan.apps.empty());
        CHECK(sysPlan.idleApps.empty());
        CHECK(sysPlan.systemSounds);
    }

    SUBCASE("apenas sessoes paradas => secao de tocando vazia, idle preenchida")
    {
        const SessionPlan idlePlan =
            planSessions({makeSession(L"i", L"parado.exe", false)});
        CHECK(idlePlan.apps.empty());
        REQUIRE(idlePlan.idleApps.size() == 1);
        CHECK(idlePlan.idleApps[0].processName == L"parado.exe");
        CHECK(!idlePlan.systemSounds);
    }

    SUBCASE("sem sessoes => nada")
    {
        const SessionPlan empty = planSessions({});
        CHECK(empty.apps.empty());
        CHECK(empty.idleApps.empty());
        CHECK(!empty.systemSounds);
    }
}

// ---------------------------------------------------------------------------
// Dropdown de roteamento — itens e selecao inicial
// ---------------------------------------------------------------------------
TEST_CASE("routeItems comeca pelo padrao do sistema e lista as saidas")
{
    std::vector<DeviceInfo> outputs;
    outputs.push_back(makeDevice(L"d1", L"Alto-falantes"));
    outputs.push_back(makeDevice(L"d2", L"Headphones USB"));

    const std::vector<std::wstring> items = routeItems(outputs);
    REQUIRE(items.size() == 3);
    CHECK(items[0] == std::wstring(tr(L"mixer.defaultOutputLabel")));
    CHECK(items[1] == L"Alto-falantes");
    CHECK(items[2] == L"Headphones USB");
}

TEST_CASE("routeSelectionIndex seleciona a saida roteada do app")
{
    std::vector<DeviceInfo> outputs;
    outputs.push_back(makeDevice(L"d1", L"Alto-falantes"));
    outputs.push_back(makeDevice(L"d2", L"HDMI"));
    outputs.push_back(makeDevice(L"d3", L"Headphones"));

    SUBCASE("deviceId vazio => item 0 (padrao do sistema)")
    {
        CHECK(routeSelectionIndex(outputs, L"") == 0);
    }

    SUBCASE("deviceId conhecido => indice da saida (+1 pelo item de padrao)")
    {
        CHECK(routeSelectionIndex(outputs, L"d1") == 1);
        CHECK(routeSelectionIndex(outputs, L"d3") == 3);
    }

    SUBCASE("deviceId desconhecido => volta ao padrao")
    {
        CHECK(routeSelectionIndex(outputs, L"outra") == 0);
    }

    SUBCASE("sem saidas => padrao")
    {
        std::vector<DeviceInfo> none;
        CHECK(routeSelectionIndex(none, L"d1") == 0);
    }
}

// ---------------------------------------------------------------------------
// Glifo das saidas
// ---------------------------------------------------------------------------
TEST_CASE("outputGlyphFor usa fones para nomes de headphone")
{
    CHECK(outputGlyphFor(L"Headphones USB") == tokens::kIconHeadphones);
    CHECK(outputGlyphFor(L"fone de ouvido") == tokens::kIconHeadphones);
    CHECK(outputGlyphFor(L"Auriculares BT") == tokens::kIconHeadphones);
    CHECK(outputGlyphFor(L"AirPods Pro") == tokens::kIconHeadphones);
    CHECK(outputGlyphFor(L"Alto-falantes (Realtek)") == tokens::kIconSpeaker);
    CHECK(outputGlyphFor(L"Monitor (HDMI)") == tokens::kIconSpeaker);
}

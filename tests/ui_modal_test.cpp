// ============================================================================
// Testes dos modais popup (Track I) — apenas logica pura, SEM criacao de HWND.
// ============================================================================
#include "doctest.h"

#include "ui/windows/modal_common.h"

#include <string>
#include <vector>

using namespace soundint;
using namespace soundint::ui;
using namespace soundint::ui::modal;

namespace {

constexpr float kMargin = kPopupMargin;   // 12

bool containedIn(const Rect& rect, const RECT& area, float margin = kMargin)
{
    const float eps = 0.001f;
    return rect.x >= static_cast<float>(area.left) + margin - eps &&
           rect.y >= static_cast<float>(area.top) + margin - eps &&
           rect.x + rect.w <= static_cast<float>(area.right) - margin + eps &&
           rect.y + rect.h <= static_cast<float>(area.bottom) - margin + eps;
}

DeviceInfo makeDevice(const wchar_t* id, const wchar_t* name)
{
    DeviceInfo device;
    device.id = id;
    device.friendlyName = name;
    device.flow = Flow::Render;
    device.active = true;
    return device;
}

}  // namespace

// ---------------------------------------------------------------------------
// computePopupRect — canto inferior direito, margem e clamp
// ---------------------------------------------------------------------------
TEST_CASE("computePopupRect: nasce no canto inferior direito com margem 12")
{
    const RECT workArea{0, 0, 1920, 1040};
    const Rect rect = computePopupRect({10, 10}, {300.f, 150.f}, workArea);

    DOCTEST_CHECK(rect.x == doctest::Approx(1920.f - kMargin - 300.f));
    DOCTEST_CHECK(rect.y == doctest::Approx(1040.f - kMargin - 150.f));
    DOCTEST_CHECK(rect.w == doctest::Approx(300.f));
    DOCTEST_CHECK(rect.h == doctest::Approx(150.f));
    DOCTEST_CHECK(containedIn(rect, workArea));
}

TEST_CASE("computePopupRect: work area deslocada mantem a margem relativa")
{
    const RECT workArea{2560, 100, 2560 + 1280, 100 + 720};
    const Rect rect = computePopupRect({3000, 400}, {320.f, 200.f}, workArea);

    DOCTEST_CHECK(rect.x == doctest::Approx(2560.f + 1280.f - kMargin - 320.f));
    DOCTEST_CHECK(rect.y == doctest::Approx(100.f + 720.f - kMargin - 200.f));
    DOCTEST_CHECK(containedIn(rect, workArea));
}

TEST_CASE("computePopupRect: clamp na borda esquerda e superior")
{
    const RECT workArea{0, 0, 200, 120};
    const Rect rect = computePopupRect({0, 0}, {400.f, 400.f}, workArea);

    DOCTEST_CHECK(rect.x == doctest::Approx(kMargin));
    DOCTEST_CHECK(rect.y == doctest::Approx(kMargin));
    DOCTEST_CHECK(rect.w == doctest::Approx(200.f - kMargin * 2.f));
    DOCTEST_CHECK(rect.h == doctest::Approx(120.f - kMargin * 2.f));
    DOCTEST_CHECK(containedIn(rect, workArea));
}

TEST_CASE("computePopupRect: popup sempre 100% contido (4 bordas)")
{
    const RECT workArea{-1920, -200, 1920, 860};
    const float sizes[] = {10.f, 50.f, 300.f, 1000.f, 3840.f, 8000.f};
    for (const float w : sizes) {
        for (const float h : sizes) {
            const Rect rect = computePopupRect({0, 0}, {w, h}, workArea);
            CHECK(containedIn(rect, workArea));
        }
    }
}

TEST_CASE("computePopupRect: sem work area util ancora no proprio anchor")
{
    const RECT degenerate{100, 100, 100, 100};
    const Rect rect = computePopupRect({500, 400}, {200.f, 100.f}, degenerate);

    DOCTEST_CHECK(rect.x == doctest::Approx(300.f));
    DOCTEST_CHECK(rect.y == doctest::Approx(300.f));
    DOCTEST_CHECK(rect.w == doctest::Approx(200.f));
    DOCTEST_CHECK(rect.h == doctest::Approx(100.f));
}

// ---------------------------------------------------------------------------
// Fila FIFO (capacidade 8) e ciclo de vida
// ---------------------------------------------------------------------------
TEST_CASE("fila: ordem FIFO")
{
    ModalQueue queue;
    DOCTEST_CHECK(queue.empty());

    for (int i = 0; i < 3; ++i) {
        DeviceInfo device = makeDevice(std::to_wstring(i).c_str(), L"Dev");
        CHECK_FALSE(queue.push(PendingModal::forDevice(device)));
    }
    DOCTEST_CHECK(queue.size() == 3);

    for (int i = 0; i < 3; ++i) {
        DOCTEST_CHECK(!queue.empty());
        DOCTEST_CHECK(queue.front().device.id == std::to_wstring(i));
        queue.pop();
    }
    DOCTEST_CHECK(queue.empty());
}

TEST_CASE("fila: capacidade 8 descarta o mais antigo")
{
    ModalQueue queue;
    for (int i = 0; i < 10; ++i) {
        SessionInfo session;
        session.pid = static_cast<uint32_t>(i);
        const bool dropped = queue.push(PendingModal::forApp(session));
        DOCTEST_CHECK(dropped == (i >= static_cast<int>(kQueueCapacity)));
    }

    DOCTEST_CHECK(queue.size() == kQueueCapacity);
    DOCTEST_CHECK(queue.front().app.pid == 2);   // 0 e 1 descartados
    while (!queue.empty()) {
        queue.pop();
    }
    DOCTEST_CHECK(queue.empty());
}

TEST_CASE("fila global: init sem janela + shutdown limpa tudo")
{
    shutdown();   // deixa o estado interno limpo antes do teste

    init(ModalHost{});
    DeviceInfo device = makeDevice(L"id-1", L"Fone Bluetooth");
    pendingQueue().push(PendingModal::forDevice(device));
    pendingQueue().push(PendingModal::forApp(SessionInfo{}));
    DOCTEST_CHECK(pendingQueue().size() == 2);

    shutdown();
    DOCTEST_CHECK(pendingQueue().empty());
    DOCTEST_CHECK_FALSE(isVisible());
}

TEST_CASE("request sem init chamado e no-op")
{
    shutdown();   // initialized = false

    DeviceInfo device = makeDevice(L"id-2", L"Alto-falante");
    SessionInfo session;
    session.pid = 42;
    requestNewDevice(device);
    requestNewApp(session);

    DOCTEST_CHECK(pendingQueue().empty());
    DOCTEST_CHECK_FALSE(isVisible());
}

// ---------------------------------------------------------------------------
// Utilidades puras
// ---------------------------------------------------------------------------
TEST_CASE("arrivalPatternFrom: trim + colapso de espacos internos")
{
    DeviceInfo device = makeDevice(L"id", L"   JBL    Tune   500BT  ");
    DOCTEST_CHECK(arrivalPatternFrom(device) == L"JBL Tune 500BT");

    DeviceInfo blank = makeDevice(L"id", L"   \t ");
    DOCTEST_CHECK(arrivalPatternFrom(blank).empty());
}

TEST_CASE("normalizeProcessName: minusculo e sem espacos")
{
    DOCTEST_CHECK(normalizeProcessName(L"  Spotify.EXE ") == L"spotify.exe");
    DOCTEST_CHECK(normalizeProcessName(L"") == L"");
}

TEST_CASE("initialOutputIndex: device atual, vazio e desconhecido")
{
    std::vector<DeviceInfo> outputs;
    outputs.push_back(makeDevice(L"dev-a", L"Alto-falantes"));
    outputs.push_back(makeDevice(L"dev-b", L"Headphones USB"));

    DOCTEST_CHECK(initialOutputIndex(outputs, L"") == 0);          // padrao do sistema
    DOCTEST_CHECK(initialOutputIndex(outputs, L"dev-a") == 1);
    DOCTEST_CHECK(initialOutputIndex(outputs, L"dev-b") == 2);
    DOCTEST_CHECK(initialOutputIndex(outputs, L"dev-z") == 0);     // desconhecido
    DOCTEST_CHECK(initialOutputIndex({}, L"dev-a") == 0);          // sem endpoints
}

TEST_CASE("deviceGlyphFor: headphones quando o nome sugere fone")
{
    DOCTEST_CHECK(deviceGlyphFor(L"Headphones (Realtek)") ==
                  std::wstring(tokens::kIconHeadphones));
    DOCTEST_CHECK(deviceGlyphFor(L"fone bluetooth JBL") ==
                  std::wstring(tokens::kIconHeadphones));
    DOCTEST_CHECK(deviceGlyphFor(L"Auriculares Apple") ==
                  std::wstring(tokens::kIconHeadphones));
    DOCTEST_CHECK(deviceGlyphFor(L"AirPods Pro") ==
                  std::wstring(tokens::kIconHeadphones));
    DOCTEST_CHECK(deviceGlyphFor(L"Alto-falantes internos") ==
                  std::wstring(tokens::kIconSpeaker));
    DOCTEST_CHECK(deviceGlyphFor(L"") == std::wstring(tokens::kIconSpeaker));
}

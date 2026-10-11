// ============================================================================
// Pagina Geral (Track K): popups, bandeja, inicializacao, tema e idioma.
// Linhas agrupadas em secoes com cabecalho (Notificacoes, Comportamento,
// Inicializacao, Aparencia e idioma) + status de falha da inicializacao.
// ============================================================================
#include "ui/windows/settings/settings_page.h"

#include "core/store.h"
#include "ui/i18n.h"

#include <algorithm>

namespace soundint::ui::settings {
namespace {

constexpr size_t kToggleCount = 5;   // 4 flags comuns + startWithWindows

using BoolSetting = bool soundint::Settings::*;

constexpr BoolSetting kFlags[4] = {
    &soundint::Settings::popupOnNewDevice,
    &soundint::Settings::popupOnNewApp,
    &soundint::Settings::rememberNewAppChoice,
    &soundint::Settings::closeToTray,
};

// Identificadores de linha no layout (indices de kSectionRows).
constexpr size_t kRowTheme = kToggleCount;
constexpr size_t kRowLanguage = kToggleCount + 1;
constexpr size_t kNoRow = static_cast<size_t>(-1);

constexpr float kSectionGap = 12.f;   // entre secoes
constexpr float kSectionHeadH = 20.f;
constexpr float kSectionHeadGap = 4.f;

// Secoes: cabecalho + linhas na ordem de exibicao (toggles 0..4, depois os
// dropdowns de Tema/Idioma). A ordem das paginas e dos toggles nao muda.
constexpr size_t kSectionCount = 4;
constexpr size_t kStartupSection = 2;   // recebe a linha de status

constexpr size_t kSectionRows[kSectionCount][2] = {
    {0, 1},
    {2, 3},
    {4, kNoRow},
    {kRowTheme, kRowLanguage},
};

constexpr const wchar_t* kSectionKeys[kSectionCount] = {
    L"settings.general.section.notifications",
    L"settings.general.section.behavior",
    L"settings.general.section.startup",
    L"settings.general.section.appearance",
};

class GeneralPage : public SettingsPage {
public:
    GeneralPage()
    {
        titleKey_ = L"settings.general.title";
        for (size_t i = 0; i < kToggleCount; ++i) {
            controls_.push_back(&toggles_[i]);
        }
        // Idioma ANTES de Tema em controls_: o popup do Tema (linha acima) e
        // desenhado depois do campo Idioma e o hit-test reverso da base da
        // precedencia ao dropdown aberto — corrige o bug Tema x Idioma.
        controls_.push_back(&languageDropdown_);
        controls_.push_back(&themeDropdown_);
        wire();
        refresh();
    }

protected:
    void retranslateImpl() override;
    void syncImpl() override;
    void layoutImpl(const Rect& area, float scale) override;
    void renderImpl(IRenderTarget& rt, const tokens::Palette& palette) override;

private:
    void wire();

    controls::Toggle toggles_[kToggleCount];
    controls::Dropdown themeDropdown_;
    controls::Dropdown languageDropdown_;
    std::wstring labels_[kToggleCount];
    std::wstring themeLabel_;
    std::wstring languageLabel_;
    std::wstring sectionTitles_[kSectionCount];
    std::wstring startupFailedText_;
    Rect labelRects_[kToggleCount]{};
    Rect themeLabelRect_{};
    Rect languageLabelRect_{};
    Rect sectionRects_[kSectionCount]{};
    Rect statusRect_{};
    bool startupFailed_ = false;
};

void GeneralPage::wire()
{
    for (size_t i = 0; i < 4; ++i) {
        const BoolSetting flag = kFlags[i];
        toggles_[i].setOnChanged([flag](bool value) {
            auto& store = core::Store::instance();
            store.settings().*flag = value;
            store.save();
            requestRender();
        });
    }

    toggles_[4].setOnChanged([this](bool value) {
        auto& store = core::Store::instance();
        const bool ok = applyStartup(value);
        // Falhou: o registro continua no estado anterior — o toggle volta e
        // a linha de status avisa (antes a mudanca sumia em silencio).
        store.settings().startWithWindows = ok ? value : !value;
        toggles_[4].setValue(ok ? value : !value);
        startupFailed_ = !ok;
        store.save();
        requestRender();
    });

    themeDropdown_.setOnChanged([](size_t index) {
        auto& store = core::Store::instance();
        store.settings().theme = (index <= 2) ? static_cast<int>(index) : 0;
        store.save();
        requestRender();
    });

    languageDropdown_.setOnChanged([](size_t index) {
        static const wchar_t* kCodes[] = {L"auto", L"pt-BR", L"en"};
        const wchar_t* code = (index < 3) ? kCodes[index] : kCodes[0];
        auto& store = core::Store::instance();
        store.settings().language = code;
        store.save();
        setLanguage(code);
        notifyLanguageChanged();
    });
}

void GeneralPage::retranslateImpl()
{
    labels_[0] = tr(L"settings.general.popupOnNewDevice");
    labels_[1] = tr(L"settings.general.popupOnNewApp");
    labels_[2] = tr(L"settings.general.rememberNewAppChoice");
    labels_[3] = tr(L"settings.general.closeToTray");
    labels_[4] = tr(L"settings.general.startWithWindows");
    themeLabel_ = tr(L"settings.general.theme");
    languageLabel_ = tr(L"settings.general.language");
    for (size_t sec = 0; sec < kSectionCount; ++sec) {
        sectionTitles_[sec] = tr(kSectionKeys[sec]);
    }
    startupFailedText_ = tr(L"settings.general.startupFailed");

    themeDropdown_.setItems({tr(L"settings.general.theme.system"),
                             tr(L"settings.general.theme.light"),
                             tr(L"settings.general.theme.dark")});
    languageDropdown_.setItems({tr(L"settings.general.language.auto"),
                                tr(L"settings.general.language.pt"),
                                tr(L"settings.general.language.en")});
}

void GeneralPage::syncImpl()
{
    const soundint::Settings& settings = core::Store::instance().settings();
    toggles_[0].setValue(settings.popupOnNewDevice);
    toggles_[1].setValue(settings.popupOnNewApp);
    toggles_[2].setValue(settings.rememberNewAppChoice);
    toggles_[3].setValue(settings.closeToTray);
    toggles_[4].setValue(settings.startWithWindows);

    const int theme = (settings.theme >= 0 && settings.theme <= 2) ? settings.theme : 0;
    themeDropdown_.setSelectedIndex(static_cast<size_t>(theme));

    size_t languageIndex = 0;
    if (settings.language == L"pt-BR") {
        languageIndex = 1;
    } else if (settings.language == L"en") {
        languageIndex = 2;
    }
    languageDropdown_.setSelectedIndex(languageIndex);
}

void GeneralPage::layoutImpl(const Rect& area, float /*scale*/)
{
    const float y0 = area.y - scrollY_;
    const float rowH = s(tokens::kRowHeight);
    const float headH = s(kSectionHeadH);
    // Trilha de 48px universal: mesma largura das paginas de Regras/Atalhos.
    const float toggleW = s(48.f);
    const float toggleH = s(tokens::kControlHeight);
    const float fieldW = s(220.f);
    // fieldH acompanha a geometria interna do Dropdown (nao escala por enquanto
    // — o controle desenha em unidades de 96 DPI; reportado para a U1).
    const float fieldH = static_cast<float>(tokens::kControlHeight);

    // Zera os retangulos: so as linhas posicionadas abaixo sao desenhadas.
    for (Rect& rect : labelRects_) {
        rect = Rect{};
    }
    themeLabelRect_ = Rect{};
    languageLabelRect_ = Rect{};
    statusRect_ = Rect{};

    float y = y0;
    for (size_t sec = 0; sec < kSectionCount; ++sec) {
        sectionRects_[sec] = {area.x, y, area.w, headH};
        y += headH + s(kSectionHeadGap);

        for (size_t i = 0; i < 2; ++i) {
            const size_t row = kSectionRows[sec][i];
            if (row == kNoRow) {
                continue;
            }
            if (row < kToggleCount) {
                const float labelW = area.w - toggleW - s(16.f);
                labelRects_[row] = {area.x, y, labelW, rowH};
                toggles_[row].setBounds({area.x + area.w - toggleW,
                                         y + (rowH - toggleH) * 0.5f, toggleW,
                                         toggleH});
            } else if (row == kRowTheme) {
                themeLabelRect_ = {area.x, y, area.w - fieldW - s(16.f), rowH};
                const float expandedH = themeDropdown_.expanded()
                                            ? themeDropdown_.requiredHeight()
                                            : fieldH;
                themeDropdown_.setBounds({area.x + area.w - fieldW,
                                         y + (rowH - fieldH) * 0.5f, fieldW,
                                         expandedH});
            } else {
                languageLabelRect_ = {area.x, y, area.w - fieldW - s(16.f), rowH};
                const float expandedH = languageDropdown_.expanded()
                                            ? languageDropdown_.requiredHeight()
                                            : fieldH;
                languageDropdown_.setBounds({area.x + area.w - fieldW,
                                            y + (rowH - fieldH) * 0.5f, fieldW,
                                            expandedH});
            }
            y += rowH;
        }

        // Status da inicializacao: linha extra discreta sob o toggle (so
        // quando a ultima tentativa de gravar no registro falhou).
        if (sec == kStartupSection && startupFailed_) {
            statusRect_ = {area.x, y, area.w, s(20.f)};
            y += s(20.f) + s(4.f);
        }

        y += s(kSectionGap);
    }

    float contentH = y - y0 - s(kSectionGap);

    // Popup aberto extrapola a ultima linha: o conteudo cresce para que o
    // scroll da base consiga revela-lo inteiro (veja SettingsPage::layout).
    if (themeDropdown_.expanded()) {
        contentH = std::max(contentH,
                           (themeDropdown_.bounds().y + themeDropdown_.bounds().h) - y0);
    }
    if (languageDropdown_.expanded()) {
        contentH = std::max(
            contentH, (languageDropdown_.bounds().y + languageDropdown_.bounds().h) - y0);
    }
    contentH_ = contentH;
}

void GeneralPage::renderImpl(IRenderTarget& rt, const tokens::Palette& palette)
{
    // Cabecalhos das secoes (caption semibold, cor secundaria).
    for (size_t sec = 0; sec < kSectionCount; ++sec) {
        drawText(rt, sectionRects_[sec], sectionTitles_[sec], palette,
                 tokens::kFontCaption, tokens::kWeightSemibold, palette.textSecondary);
    }

    for (size_t i = 0; i < kToggleCount; ++i) {
        if (labelRects_[i].h <= 0.f) {
            continue;
        }
        drawText(rt, labelRects_[i], labels_[i], palette, tokens::kFontBody,
                 tokens::kWeightRegular, palette.textPrimary);
    }
    if (themeLabelRect_.h > 0.f) {
        drawText(rt, themeLabelRect_, themeLabel_, palette, tokens::kFontBody,
                 tokens::kWeightRegular, palette.textPrimary);
    }
    if (languageLabelRect_.h > 0.f) {
        drawText(rt, languageLabelRect_, languageLabel_, palette, tokens::kFontBody,
                 tokens::kWeightRegular, palette.textPrimary);
    }
    if (startupFailed_ && statusRect_.h > 0.f) {
        drawText(rt, statusRect_, startupFailedText_, palette, tokens::kFontCaption,
                 tokens::kWeightRegular, palette.danger);
    }
}

}  // namespace

std::unique_ptr<SettingsPage> createGeneralPage()
{
    return std::make_unique<GeneralPage>();
}

}  // namespace soundint::ui::settings

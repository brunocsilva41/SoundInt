// ============================================================================
// Pagina Atualizacoes (U3): autoCheckUpdates, betaChannel e verificacao sob
// demanda via host().checkForUpdates (assincrono; resultado chega por
// kMsgUpdateResult na thread principal).
//
// Maquina de estados real: idle -> checking (botao desabilitado) ->
// disponivel (com acao "Baixar atualizacao") | atualizado | erro.
// Mapeamento corrigido: done(false, "Erro: ...") vira estado ERRO visivel —
// nunca "versao mais recente". A versao instalada nao aparece aqui (Sobre
// ja mostra; P2: sem duplicacao).
// ============================================================================
#include "ui/windows/settings/settings_page.h"

#include "core/store.h"
#include "ui/i18n.h"
#include "ui/renderer.h"

#include <shellapi.h>

namespace soundint::ui::settings {
namespace {

// O canal SettingsHost so repassa a versao encontrada; a URL direta do
// instalador vive no manifest (update_service) e nao chega aqui. Abre a
// pagina da release mais recente no navegador padrao.
constexpr wchar_t kReleasesUrl[] =
    L"https://github.com/brunocsilva41/SoundInt/releases/latest";

// Larguras/alturas logicas (96 DPI) padronizadas com as demais paginas.
constexpr float kCheckW = 140.f;
constexpr float kDownloadW = 150.f;
constexpr float kToggleW = 48.f;   // largura padronizada
constexpr float kStatusRowH = 28.f;

class UpdatesPage : public SettingsPage {
public:
    UpdatesPage()
    {
        titleKey_ = L"settings.updates.title";
        autoCheckToggle_.setOnChanged([](bool value) {
            auto& store = core::Store::instance();
            store.settings().autoCheckUpdates = value;
            store.save();
            requestRender();
        });
        betaToggle_.setOnChanged([](bool value) {
            auto& store = core::Store::instance();
            store.settings().betaChannel = value;
            store.save();
            requestRender();
        });
        checkButton_.setVariant(controls::Button::Variant::Primary);
        checkButton_.setOnClick([this]() { checkNow(); });
        downloadButton_.setVariant(controls::Button::Variant::Primary);
        downloadButton_.setGlyph(tokens::kIconDownload);
        downloadButton_.setVisible(false);
        downloadButton_.setOnClick([]() {
            // PT: abre a release mais recente no navegador (o manifest carrega
            // a URL direta do instalador, mas o canal da UI nao a expoe).
            ShellExecuteW(nullptr, L"open", kReleasesUrl, nullptr, nullptr,
                          SW_SHOWNORMAL);
        });

        controls_.push_back(&autoCheckToggle_);
        controls_.push_back(&betaToggle_);
        controls_.push_back(&checkButton_);
        controls_.push_back(&downloadButton_);
        refresh();
    }

    // found=true -> disponivel; found=false + info -> ERRO (mostra a mensagem);
    // found=false sem info -> em dia. Erro NUNCA vira "versao mais recente".
    void updateResult(bool found, const std::wstring& info) override
    {
        if (found) {
            state_ = State::Available;
            info_ = info;
        } else if (!info.empty()) {
            state_ = State::Error;
            info_ = info;
        } else {
            state_ = State::UpToDate;
            info_.clear();
        }
        syncCheckButton();
        requestRender();
    }

protected:
    void retranslateImpl() override
    {
        autoCheckLabel_ = tr(L"settings.updates.autoCheck");
        betaLabel_ = tr(L"settings.updates.betaChannel");
        autoCheckToggle_.setAccessibleName(autoCheckLabel_);
        betaToggle_.setAccessibleName(betaLabel_);
        checkButton_.setGlyph(tokens::kIconRefresh);
        downloadButton_.setText(tr(L"settings.updates.download"));
        syncCheckButton();
    }

    void syncImpl() override
    {
        const soundint::Settings& settings = core::Store::instance().settings();
        autoCheckToggle_.setValue(settings.autoCheckUpdates);
        betaToggle_.setValue(settings.betaChannel);
    }

    void layoutImpl(const Rect& area, float scale) override;
    void renderImpl(IRenderTarget& rt, const tokens::Palette& palette) override;

private:
    enum class State { Idle, Checking, Available, UpToDate, Error, NoService };

    void checkNow();
    void syncCheckButton();
    std::wstring statusText() const;

    controls::Toggle autoCheckToggle_;
    controls::Toggle betaToggle_;
    controls::Button checkButton_;
    controls::Button downloadButton_;

    std::wstring autoCheckLabel_;
    std::wstring betaLabel_;
    std::wstring info_;

    State state_ = State::Idle;
    Rect autoRect_{};
    Rect betaRect_{};
    Rect statusRect_{};
};

void UpdatesPage::checkNow()
{
    // Sem reentry: um segundo clique durante a checagem lancaria outra
    // thread e os dois resultados briguariam pelo mesmo info_/state_.
    if (state_ == State::Checking) {
        return;
    }
    if (!host().checkForUpdates) {
        state_ = State::NoService;
        info_.clear();
        syncCheckButton();
        requestRender();
        return;
    }
    state_ = State::Checking;
    info_.clear();
    syncCheckButton();
    requestRender();
    host().checkForUpdates(
        [](bool found, const std::wstring& info) { postUpdateResult(found, info); });
}

// Estado do botao principal acompanha a maquina de estados: durante a
// checagem fica desabilitado com texto "Verificando..." (antes nao havia
// loading/disabled nenhum).
void UpdatesPage::syncCheckButton()
{
    const bool checking = (state_ == State::Checking);
    checkButton_.setEnabled(!checking);
    checkButton_.setText(tr(checking ? L"settings.updates.checking"
                                     : L"settings.updates.checkNow"));
    downloadButton_.setVisible(state_ == State::Available);
}

std::wstring UpdatesPage::statusText() const
{
    switch (state_) {
        case State::Checking:
            return tr(L"settings.updates.checking");
        case State::Available: {
            std::wstring text = tr(L"settings.updates.available");
            if (!info_.empty()) {
                text += L' ';
                text += info_;
            }
            return text;
        }
        case State::UpToDate:
            return tr(L"settings.updates.upToDate");
        case State::Error:
            // Mensagem vinda do modulo de update (ex.: "Erro: sem rede").
            return info_.empty() ? std::wstring(tr(L"settings.updates.noService"))
                                : info_;
        case State::NoService:
            return tr(L"settings.updates.noService");
        case State::Idle:
        default:
            return std::wstring{};
    }
}

void UpdatesPage::layoutImpl(const Rect& area, float /*scale*/)
{
    const float y0 = area.y - scrollY_;
    const float rowH = s(tokens::kRowHeight);
    const float toggleW = s(kToggleW);
    const float toggleH = s(tokens::kControlHeight);
    const float labelW = area.w - toggleW - s(16.f);
    const float buttonH = s(tokens::kControlHeight);

    float y = y0;
    autoRect_ = {area.x, y, labelW, rowH};
    autoCheckToggle_.setBounds({area.x + area.w - toggleW, y + (rowH - toggleH) * 0.5f,
                                toggleW, toggleH});
    y += rowH;

    betaRect_ = {area.x, y, labelW, rowH};
    betaToggle_.setBounds({area.x + area.w - toggleW, y + (rowH - toggleH) * 0.5f,
                           toggleW, toggleH});
    y += rowH + s(8.f);

    checkButton_.setBounds({area.x, y, s(kCheckW), buttonH});
    y += buttonH + s(4.f);

    // Status na posicao padronizada (abaixo da linha de acao), reservado.
    statusRect_ = {area.x, y + s(4.f), area.w, s(20.f)};
    y += s(kStatusRowH);

    if (downloadButton_.visible()) {
        downloadButton_.setBounds({area.x, y, s(kDownloadW), buttonH});
        y += buttonH + s(8.f);
    }

    contentH_ = y - y0;
}

void UpdatesPage::renderImpl(IRenderTarget& rt, const tokens::Palette& palette)
{
    drawText(rt, autoRect_, autoCheckLabel_, palette, tokens::kFontBody,
             tokens::kWeightRegular, palette.textPrimary);
    drawText(rt, betaRect_, betaLabel_, palette, tokens::kFontBody,
             tokens::kWeightRegular, palette.textPrimary);

    const std::wstring status = statusText();
    if (!status.empty()) {
        // Cor semantica: verde = em dia/disponivel, accent = verificando,
        // vermelho = erro/indisponivel.
        Color color = palette.textSecondary;
        switch (state_) {
            case State::Available:
            case State::UpToDate:
                color = palette.success;
                break;
            case State::Checking:
                color = palette.accent;
                break;
            case State::Error:
            case State::NoService:
                color = palette.danger;
                break;
            case State::Idle:
            default:
                break;
        }
        drawText(rt, statusRect_, status, palette, tokens::kFontBody,
                 tokens::kWeightRegular, color);
    }
}

}  // namespace

std::unique_ptr<SettingsPage> createUpdatesPage()
{
    return std::make_unique<UpdatesPage>();
}

}  // namespace soundint::ui::settings

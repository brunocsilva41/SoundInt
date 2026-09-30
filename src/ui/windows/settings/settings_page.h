// ============================================================================
// Infra interna das paginas da janela de Configuracoes (Track K).
// Nao e contrato: so consumida por src/ui/windows/settings/**.
// ============================================================================
#pragma once

#include "ui/controls/controls.h"
#include "ui/renderer.h"
#include "ui/windows/settings/settings_window.h"

#include <memory>
#include <string>
#include <vector>

#include <windows.h>

namespace soundint::ui::settings {

// Host injetado (mesmo objeto preenchido por init()).
SettingsHost& host();

// Mensagem assincrona de resultado de verificacao de atualizacao. O callback
// pode chegar de outra thread: enfileira + PostMessage; a janela drena no
// message loop da thread principal.
inline constexpr UINT kMsgUpdateResult = WM_APP + 61;

void postUpdateResult(bool found, const std::wstring& info);
bool takeUpdateResult(bool& found, std::wstring& info);

// A janela atualiza TODAS as paginas (retranslate apos troca de idioma).
void notifyLanguageChanged();

// ---------------------------------------------------------------------------
// TextField: campo de texto simples (MVP: append/backspace, sem selecao).
// ---------------------------------------------------------------------------
class TextField : public controls::InteractiveControl {
public:
    void setText(std::wstring text) { text_ = std::move(text); }
    const std::wstring& text() const { return text_; }
    void setPlaceholder(std::wstring placeholder) { placeholder_ = std::move(placeholder); }
    void setEditable(bool editable) { editable_ = editable; }
    void setScale(float scale) { scale_ = scale; }
    void append(wchar_t c);
    void eraseBack();

    void render(IRenderTarget& rt, const tokens::Palette& palette) override;
    bool onPointer(const PointerEvent& e) override;
    Size measure(float constraintWidth) override;

    static constexpr size_t kMaxLength = 160;

private:
    std::wstring text_;
    std::wstring placeholder_;
    float scale_ = 1.f;
    bool editable_ = true;
};

// ---------------------------------------------------------------------------
// Base das paginas
// ---------------------------------------------------------------------------
class SettingsPage {
public:
    virtual ~SettingsPage();

    const wchar_t* titleKey() const { return titleKey_; }

    // Ciclo de vida ---------------------------------------------------------
    // Reaplica tr() aos textos e recria listas dinamicas.
    void retranslate() { retranslateImpl(); }
    // Le o Store e atualiza estados dos controles.
    void syncFromStore() { syncImpl(); }
    void refresh();   // retranslate() + syncFromStore()

    // Layout/pintura --------------------------------------------------------
    void layout(const Rect& area, float scale);
    void render(IRenderTarget& rt, const tokens::Palette& palette);

    // Entrada ---------------------------------------------------------------
    // capture: controle que passa a receber Move/Up (gerenciado pela janela).
    bool pointer(const PointerEvent& e, Control*& capture);
    bool key(const KeyEvent& e);
    bool character(wchar_t c);
    bool rawKey(UINT msg, WPARAM w);
    // Cola texto no campo focado (Ctrl+V); true se havia campo editavel.
    bool paste(const std::wstring& text);
    // Chamado quando a janela sai da pagina (ex.: cancela gravacao).
    virtual void onHide() {}

    // Resultado do checkForUpdates (somente pagina Atualizacoes).
    virtual void updateResult(bool /*found*/, const std::wstring& /*info*/) {}

protected:
    // Implementacao por pagina ------------------------------------------------
    virtual void retranslateImpl() = 0;
    virtual void syncImpl() = 0;
    virtual void layoutImpl(const Rect& area, float scale) = 0;
    virtual void renderImpl(IRenderTarget& rt, const tokens::Palette& palette) = 0;
    virtual bool pointerImpl(const PointerEvent&) { return false; }
    virtual bool keyImpl(const KeyEvent&) { return false; }
    virtual bool charImpl(wchar_t) { return false; }
    virtual bool rawKeyImpl(UINT, WPARAM) { return false; }
    // Altura total do conteudo (para scroll); padrao = nao rola.
    virtual float contentHeightImpl() const { return 0.f; }

    // Helpers de desenho (escalam por dpiScale) ------------------------------
    float s(float v) const { return v * scale_; }
    TextStyle textStyle(const tokens::Palette& palette, float fontSize, int weight,
                        Color color, TextStyle::Align align = TextStyle::Align::Left) const;
    void drawText(IRenderTarget& rt, const Rect& box, const std::wstring& text,
                  const tokens::Palette& palette, float fontSize, int weight,
                  Color color, TextStyle::Align align = TextStyle::Align::Left) const;
    TextField* addField(TextField* field);   // registra e devolve (encadeavel)
    void setScrollY(float scrollY);
    float maxScrollY() const;

    const wchar_t* titleKey_ = nullptr;
    Rect area_{};                  // area da pagina (px fisicos)
    float scale_ = 1.f;
    float scrollY_ = 0.f;
    float contentH_ = 0.f;         // preenchido por layoutImpl
    std::vector<Control*> controls_;
    std::vector<TextField*> fields_;
};

// Fabricas (uma por arquivo de pagina).
std::unique_ptr<SettingsPage> createGeneralPage();
std::unique_ptr<SettingsPage> createRulesPage();
std::unique_ptr<SettingsPage> createProfilesPage();
std::unique_ptr<SettingsPage> createHotkeysPage();
std::unique_ptr<SettingsPage> createUpdatesPage();
std::unique_ptr<SettingsPage> createAboutPage();

}  // namespace soundint::ui::settings

// ============================================================================
// Pagina Atalhos (U3): lista de bindings do Store, gravacao da proxima
// combinacao (WM_KEYDOWN cru) e notificacao host().hotkeysChanged().
//
// - Hint de gravacao SOMENTE enquanto recordingId_ esta ativo (antes era
//   desenhado sempre); fora da gravacao a mesma linha vira status
//   transitorio (ex.: atalho desativado via Delete).
// - Perfis mostram o NOME REAL ("Perfil: Home"), nao "Perfil 3" por indice.
// - NENHUM callback destroi controles sincronamente (UAF): rebuilds sao
//   adiados (rowsDirty_) para o proximo layout/render. Isso cobre o caso
//   "mouse capturado no botao Gravar + tecla pressionada".
// ============================================================================
#include "ui/windows/settings/settings_page.h"

#include "ui/windows/settings/settings_undo.h"

#include "core/store.h"
#include "ui/i18n.h"
#include "ui/renderer.h"

#include <algorithm>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

namespace soundint::ui::settings {
namespace {

// Larguras logicas (96 DPI) padronizadas com as demais paginas.
constexpr float kRecordW = 110.f;
constexpr float kCaptionW = 52.f;

bool isModifierVk(UINT vk)
{
    switch (vk) {
        case VK_CONTROL:
        case VK_LCONTROL:
        case VK_RCONTROL:
        case VK_SHIFT:
        case VK_LSHIFT:
        case VK_RSHIFT:
        case VK_MENU:
        case VK_LMENU:
        case VK_RMENU:
        case VK_LWIN:
        case VK_RWIN:
            return true;
        default:
            return false;
    }
}

unsigned currentModifiers()
{
    unsigned modifiers = 0;
    if ((GetKeyState(VK_CONTROL) & 0x8000) != 0) {
        modifiers |= static_cast<unsigned>(MOD_CONTROL);
    }
    if ((GetKeyState(VK_MENU) & 0x8000) != 0) {
        modifiers |= static_cast<unsigned>(MOD_ALT);
    }
    if ((GetKeyState(VK_SHIFT) & 0x8000) != 0) {
        modifiers |= static_cast<unsigned>(MOD_SHIFT);
    }
    if ((GetKeyState(VK_LWIN) & 0x8000) != 0 || (GetKeyState(VK_RWIN) & 0x8000) != 0) {
        modifiers |= static_cast<unsigned>(MOD_WIN);
    }
    return modifiers;
}

// Nome legivel da tecla (fisica) para exibicao.
std::wstring vkName(unsigned vk)
{
    if (vk >= 0x30 && vk <= 0x39) {
        return std::wstring(1, static_cast<wchar_t>(vk));   // 0-9
    }
    if (vk >= 0x41 && vk <= 0x5A) {
        return std::wstring(1, static_cast<wchar_t>(vk));   // A-Z
    }
    if (vk >= 0x70 && vk <= 0x87) {
        return L"F" + std::to_wstring(vk - 0x6F);
    }
    switch (vk) {
        case VK_SPACE:    return L"Space";
        case VK_RETURN:   return L"Enter";
        case VK_TAB:      return L"Tab";
        case VK_ESCAPE:   return L"Esc";
        case VK_BACK:     return L"Backspace";
        case VK_DELETE:   return L"Delete";
        case VK_INSERT:   return L"Insert";
        case VK_HOME:     return L"Home";
        case VK_END:      return L"End";
        case VK_PRIOR:    return L"Page Up";
        case VK_NEXT:     return L"Page Down";
        case VK_LEFT:     return L"Left";
        case VK_RIGHT:    return L"Right";
        case VK_UP:       return L"Up";
        case VK_DOWN:     return L"Down";
        case VK_OEM_1:    return L";";
        case VK_OEM_PLUS: return L"=";
        case VK_OEM_COMMA:   return L",";
        case VK_OEM_MINUS:   return L"-";
        case VK_OEM_PERIOD:  return L".";
        case VK_OEM_2:    return L"/";
        case VK_OEM_3:    return L"`";
        case VK_OEM_4:    return L"[";
        case VK_OEM_5:    return L"\\";
        case VK_OEM_6:    return L"]";
        case VK_OEM_7:    return L"'";
        default:
            break;
    }
    const UINT ch = MapVirtualKeyW(vk, MAPVK_VK_TO_CHAR);
    if (ch >= 0x20 && ch < 0x7F) {
        return std::wstring(1, static_cast<wchar_t>(ch));
    }
    wchar_t buffer[16]{};
    swprintf_s(buffer, L"0x%02X", vk);
    return buffer;
}

std::wstring formatBinding(const soundint::HotkeyBinding& binding)
{
    if (binding.vk == 0) {
        return tr(L"settings.hotkeys.none");
    }
    std::wstring out;
    if ((binding.modifiers & static_cast<unsigned>(MOD_CONTROL)) != 0) {
        out += L"Ctrl+";
    }
    if ((binding.modifiers & static_cast<unsigned>(MOD_ALT)) != 0) {
        out += L"Alt+";
    }
    if ((binding.modifiers & static_cast<unsigned>(MOD_SHIFT)) != 0) {
        out += L"Shift+";
    }
    if ((binding.modifiers & static_cast<unsigned>(MOD_WIN)) != 0) {
        out += L"Win+";
    }
    out += vkName(binding.vk);
    return out;
}

// Rotulo amigavel por id ("mixer", "cycleOutput", "profile:<n>").
// Perfis resolvem o NOME REAL do perfil ("Perfil: Home"); indice numerico
// so aparece como fallback quando o perfil nao existe mais.
std::wstring labelForId(const std::wstring& id)
{
    if (id == L"mixer") {
        return tr(L"settings.hotkeys.mixer");
    }
    if (id == L"cycleOutput") {
        return tr(L"settings.hotkeys.cycleOutput");
    }
    static constexpr wchar_t kProfilePrefix[] = L"profile:";
    if (id.rfind(kProfilePrefix, 0) == 0) {
        std::wstring out = tr(L"settings.hotkeys.profile");
        const std::wstring suffix = id.substr(8);
        if (suffix.empty()) {
            return out;
        }
        bool numeric = true;
        for (const wchar_t c : suffix) {
            if (c < L'0' || c > L'9') {
                numeric = false;
                break;
            }
        }
        if (numeric) {
            const int index = _wtoi(suffix.c_str());
            const auto& profiles = core::Store::instance().profiles();
            const size_t pos = static_cast<size_t>(index < 0 ? 0 : index);
            if (pos < profiles.size() && !profiles[pos].name.empty()) {
                out += L": ";
                out += profiles[pos].name;
                return out;
            }
        }
        out += L' ';
        out += suffix;   // perfil inexistente/invalido: mantem o indice
        return out;
    }
    return id;
}

struct HotkeyRow {
    std::wstring id;
    std::unique_ptr<controls::Toggle> enabled;
    std::unique_ptr<controls::Button> record;
};

class HotkeysPage : public SettingsPage {
public:
    HotkeysPage()
    {
        titleKey_ = L"settings.hotkeys.title";
        rebuildRows();
        refresh();
    }

protected:
    void retranslateImpl() override
    {
        hint_ = tr(L"settings.hotkeys.hint");
        rebuildRows();
    }

    void syncImpl() override
    {
        rebuildRows();
    }

    void onHide() override
    {
        SettingsPage::onHide();   // base: fecha popups e solta o foco
        recordingId_.clear();
        requestRender();
    }

    // Conflito de re-registro (ex.: atalho ja usado por outro app): mostra na
    // linha de status. Rota: main -> notifyHotkeysResult -> janela -> pagina.
    void hotkeysResult(bool ok) override
    {
        if (!ok) {
            status_.show(StatusKind::Danger, tr(L"settings.hotkeys.conflict"));
            requestRender();
        }
    }

    bool rawKeyImpl(UINT msg, WPARAM w) override
    {
        if (recordingId_.empty()) {
            return false;
        }
        if (msg == WM_KEYUP || msg == WM_SYSKEYUP) {
            return true;   // engole o keyup da combinacao gravada
        }

        const UINT vk = static_cast<UINT>(w);
        if (isModifierVk(vk)) {
            return true;   // aguarda a tecla nao-modificadora
        }
        if (vk == VK_ESCAPE) {
            recordingId_.clear();
            status_.show(StatusKind::Info, tr(L"settings.hotkeys.cancelled"));
            requestRender();
            return true;
        }

        auto& store = core::Store::instance();
        bool unassigned = false;
        for (soundint::HotkeyBinding& binding : store.settings().hotkeys) {
            if (binding.id != recordingId_) {
                continue;
            }
            if (vk == VK_DELETE) {
                binding.vk = 0;          // desativa o atalho
                binding.modifiers = 0;
                unassigned = true;
            } else {
                binding.modifiers = currentModifiers();
                binding.vk = vk;
            }
            break;
        }
        recordingId_.clear();
        store.save();
        if (unassigned) {
            status_.show(StatusKind::Info, tr(L"settings.hotkeys.unassigned"));
        }
        // Rebuild ADIADO: destruir o botao Gravar aqui, com o mouse capturado
        // no clique anterior, seria UAF no proximo WM_MOUSE*.
        rowsDirty_ = true;
        requestRender();
        if (host().hotkeysChanged) {
            host().hotkeysChanged();
        }
        return true;
    }

    void layoutImpl(const Rect& area, float scale) override;
    void renderImpl(IRenderTarget& rt, const tokens::Palette& palette) override;

private:
    void rebuildRows();

    std::vector<std::unique_ptr<HotkeyRow>> rows_;
    std::wstring recordingId_;
    std::wstring hint_;
    PageStatus status_;

    std::vector<Rect> rowRects_;
    std::vector<Rect> labelRects_;
    std::vector<Rect> bindingRects_;
    std::vector<Rect> captionRects_;
    Rect statusRect_{};
    Rect emptyRect_{};
    bool empty_ = false;
    bool rowsDirty_ = false;
};

void HotkeysPage::rebuildRows()
{
    controls_.clear();
    rows_.clear();

    auto& store = core::Store::instance();
    const auto& hotkeys = store.settings().hotkeys;

    bool recordingStillValid = false;
    for (const soundint::HotkeyBinding& binding : hotkeys) {
        auto row = std::make_unique<HotkeyRow>();
        row->id = binding.id;

        row->enabled = std::make_unique<controls::Toggle>();
        row->enabled->setValue(binding.enabled);
        row->enabled->setAccessibleName(labelForId(binding.id));
        row->enabled->setOnChanged([id = binding.id](bool value) {
            auto& s = core::Store::instance();
            for (soundint::HotkeyBinding& hk : s.settings().hotkeys) {
                if (hk.id == id) {
                    hk.enabled = value;
                    break;
                }
            }
            s.save();
            requestRender();
            if (host().hotkeysChanged) {
                host().hotkeysChanged();
            }
        });

        std::wstring recordName = tr(L"settings.hotkeys.record");
        recordName += L": ";
        recordName += labelForId(binding.id);

        row->record = std::make_unique<controls::Button>();
        row->record->setText(tr(L"settings.hotkeys.record"));
        row->record->setGlyph(tokens::kIconKeyboard);
        row->record->setAccessibleName(recordName);
        row->record->setOnClick([this, id = binding.id]() {
            recordingId_ = id;
            requestRender();
        });

        if (recordingId_ == binding.id) {
            recordingStillValid = true;
        }
        controls_.push_back(row->enabled.get());
        controls_.push_back(row->record.get());
        rows_.push_back(std::move(row));
    }
    if (!recordingStillValid) {
        recordingId_.clear();
    }
}

void HotkeysPage::layoutImpl(const Rect& area, float /*scale*/)
{
    if (rowsDirty_) {
        rebuildRows();
        rowsDirty_ = false;
    }
    const auto& hotkeys = core::Store::instance().settings().hotkeys;
    if (rows_.size() != hotkeys.size()) {
        rebuildRows();
    }

    const float y0 = area.y - scrollY_;
    const float rowH = s(tokens::kRowHeight);
    const float controlH = s(32.f);
    const float recordW = s(kRecordW);
    const float toggleW = s(48.f);   // largura padronizada
    const float captionW = s(kCaptionW);

    float y = y0;
    empty_ = hotkeys.empty();
    rowRects_.clear();
    labelRects_.clear();
    bindingRects_.clear();
    captionRects_.clear();

    if (empty_) {
        emptyRect_ = {area.x, y + s(8.f), area.w, s(32.f)};
        y = emptyRect_.y + emptyRect_.h;
    } else {
        y += s(4.f);
        const float recordX = area.x + area.w - recordW;
        const float toggleX = recordX - s(12.f) - toggleW;
        const float captionX = toggleX - s(8.f) - captionW;
        for (size_t i = 0; i < rows_.size(); ++i) {
            rowRects_.push_back({area.x, y, area.w, rowH});
            labelRects_.push_back({area.x + s(12.f), y, s(170.f), rowH});
            bindingRects_.push_back({area.x + s(190.f), y,
                                      captionX - (area.x + s(190.f)) - s(8.f),
                                      rowH});
            captionRects_.push_back({captionX, y, captionW, rowH});
            rows_[i]->enabled->setBounds({toggleX, y + (rowH - controlH) * 0.5f,
                                          toggleW, controlH});
            rows_[i]->record->setBounds({recordX, y + (rowH - controlH) * 0.5f,
                                          recordW, controlH});
            y += rowH + s(4.f);
        }
    }

    // Linha reservada: hint de gravacao durante a captura, status transitorio
    // fora dela (antes o hint era desenhado SEMPRE).
    statusRect_ = {area.x, y + s(8.f), area.w, s(20.f)};
    y += s(28.f);
    contentH_ = y - y0;
}

void HotkeysPage::renderImpl(IRenderTarget& rt, const tokens::Palette& palette)
{
    const auto& hotkeys = core::Store::instance().settings().hotkeys;

    if (empty_) {
        drawText(rt, emptyRect_, tr(L"settings.hotkeys.empty"), palette,
                 tokens::kFontBody, tokens::kWeightRegular, palette.textSecondary,
                 TextStyle::Align::Center);
    } else {
        for (size_t i = 0; i < rowRects_.size() && i < hotkeys.size(); ++i) {
            const Rect& row = rowRects_[i];
            const bool recording = (recordingId_ == hotkeys[i].id);
            rt.fillRoundedRect(row, tokens::kRadiusMd * scale_,
                               recording ? palette.surfaceAlt : palette.surface);
            rt.strokeRoundedRect(row, tokens::kRadiusMd * scale_, 1.f,
                                 recording ? palette.accent : palette.border);

            drawText(rt, labelRects_[i], labelForId(hotkeys[i].id), palette,
                     tokens::kFontBody, tokens::kWeightRegular, palette.textPrimary);
            const std::wstring binding =
                recording ? std::wstring(tr(L"settings.hotkeys.recording"))
                          : formatBinding(hotkeys[i]);
            drawText(rt, bindingRects_[i], binding, palette, tokens::kFontBody,
                     recording ? tokens::kWeightSemibold : tokens::kWeightRegular,
                     recording ? palette.accent : palette.textSecondary);
            // Rotulo visivel do toggle da linha (antes havia so o icone mudo).
            drawText(rt, captionRects_[i], tr(L"settings.hotkeys.active"), palette,
                     tokens::kFontCaption, tokens::kWeightRegular,
                     palette.textSecondary, TextStyle::Align::Right);
        }
    }

    // Hint de gravacao SOMENTE durante a gravacao; caso contrario, status.
    if (!recordingId_.empty()) {
        drawText(rt, statusRect_, hint_, palette, tokens::kFontBody,
                 tokens::kWeightRegular, palette.accent);
    } else if (status_.active()) {
        drawText(rt, statusRect_, status_.text(), palette, tokens::kFontBody,
                 tokens::kWeightRegular, statusColor(status_.kind(), palette));
    }
}

}  // namespace

std::unique_ptr<SettingsPage> createHotkeysPage()
{
    return std::make_unique<HotkeysPage>();
}

}  // namespace soundint::ui::settings

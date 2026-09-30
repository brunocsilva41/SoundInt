// ============================================================================
// Pagina Atalhos (Track K): lista de bindings do Store, gravacao da proxima
// combinacao (WM_KEYDOWN cru) e notificacao host().hotkeysChanged().
// ============================================================================
#include "ui/windows/settings/settings_page.h"

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
        if (!suffix.empty()) {
            out += L' ';
            out += suffix;
        }
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
        recordingId_.clear();
        requestRender();
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
            requestRender();
            return true;
        }

        auto& store = core::Store::instance();
        for (soundint::HotkeyBinding& binding : store.settings().hotkeys) {
            if (binding.id != recordingId_) {
                continue;
            }
            if (vk == VK_DELETE) {
                binding.vk = 0;          // desativa o atalho
                binding.modifiers = 0;
            } else {
                binding.modifiers = currentModifiers();
                binding.vk = vk;
            }
            break;
        }
        recordingId_.clear();
        store.save();
        syncImpl();
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

    std::vector<Rect> rowRects_;
    std::vector<Rect> labelRects_;
    std::vector<Rect> bindingRects_;
    Rect hintRect_{};
    Rect emptyRect_{};
    bool empty_ = false;
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
        row->enabled->setOnChanged([this, id = binding.id](bool value) {
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

        row->record = std::make_unique<controls::Button>();
        row->record->setText(tr(L"settings.hotkeys.record"));
        row->record->setGlyph(tokens::kIconKeyboard);
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

void HotkeysPage::layoutImpl(const Rect& area, float scale)
{
    (void)scale;
    const auto& hotkeys = core::Store::instance().settings().hotkeys;
    if (rows_.size() != hotkeys.size()) {
        rebuildRows();
    }

    const float y0 = area.y - scrollY_;
    const float rowH = s(tokens::kRowHeight);
    const float controlH = s(32.f);
    const float recordW = s(140.f);
    const float toggleW = s(48.f);

    float y = y0;
    empty_ = hotkeys.empty();
    rowRects_.clear();
    labelRects_.clear();
    bindingRects_.clear();

    if (empty_) {
        emptyRect_ = {area.x, y + s(8.f), area.w, s(32.f)};
        y = emptyRect_.y + emptyRect_.h;
    } else {
        y += s(4.f);
        const float toggleX = area.x + area.w - toggleW - recordW - s(8.f);
        const float recordX = area.x + area.w - recordW;
        for (size_t i = 0; i < rows_.size(); ++i) {
            rowRects_.push_back({area.x, y, area.w, rowH});
            labelRects_.push_back({area.x + s(12.f), y, s(170.f), rowH});
            bindingRects_.push_back({area.x + s(190.f), y,
                                     toggleX - (area.x + s(190.f)) - s(8.f), rowH});
            rows_[i]->enabled->setBounds({toggleX, y + (rowH - controlH) * 0.5f,
                                          toggleW, controlH});
            rows_[i]->record->setBounds({recordX, y + (rowH - controlH) * 0.5f, recordW,
                                         controlH});
            y += rowH + s(4.f);
        }
    }

    hintRect_ = {area.x, y + s(8.f), area.w, s(20.f)};
    y = hintRect_.y + hintRect_.h;
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
        }
    }

    drawText(rt, hintRect_, hint_, palette, tokens::kFontCaption,
             tokens::kWeightRegular, palette.textSecondary);
}

}  // namespace

std::unique_ptr<SettingsPage> createHotkeysPage()
{
    return std::make_unique<HotkeysPage>();
}

}  // namespace soundint::ui::settings

// ============================================================================
// Modal "app comecou a tocar" (Track I).
// Icone do executavel, dropdown de saida (Track E) com bounds generosos e
// checkbox "lembrar escolha" (AppRule por processName).
// ============================================================================
#pragma once

#include "ui/controls/button.h"
#include "ui/controls/dropdown.h"
#include "ui/windows/modal_common.h"

namespace soundint::ui::modal {

class ModalAppWindow : public ModalWindow {
public:
    ModalAppWindow();
    ~ModalAppWindow() override;

    void setSession(const SessionInfo& session);

protected:
    Size logicalContentSize() const override;
    void layoutControls(float scale) override;
    void paint(IRenderTarget& rt, float scale, const tokens::Palette& pal) override;
    void onPrimaryAction() override;
    void onCancelAction() override;
    const wchar_t* windowClassName() const override { return L"SoundInt.ModalApp"; }
    bool preparePointerDown(controls::InteractiveControl* target) override;
    void syncState() override;

private:
    void releaseIcon();
    std::wstring chosenDeviceId() const;
    std::wstring chosenLabel() const;

    SessionInfo session_;
    std::vector<DeviceInfo> outputs_;
    HICON icon_ = nullptr;            // SHGetFileInfoW (desenho: gap do contrato)
    bool expandedSeen_ = false;       // expansao ja notificada (reframe)

    Rect iconRect_{};
    Rect titleRect_{};
    Rect nameRect_{};
    Rect labelRect_{};

    controls::Dropdown dropdown_;
    CheckBox remember_;
    controls::Button secondary_;      // "Cancelar"
    AccentButton primary_;            // "Aplicar"
};

}  // namespace soundint::ui::modal

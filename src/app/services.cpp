// ============================================================================
// Implementacao dos pontos globais do app (services.h) + estencao interna.
// Instalados no boot (main.cpp); antes do boot audioService() devolve um
// NullAudioService, para que nenhum consumidor precise checar ponteiro nulo.
// ============================================================================
#include "app/services_ext.h"

#include <utility>

namespace soundint {
namespace {

// Fachada de audio sem efeito algum (retornos neutros, nunca falha).
class NullAudioService final : public audio::IAudioService {
public:
    std::vector<DeviceInfo> devices(Flow) override { return {}; }
    std::wstring defaultDevice(Flow, Role) override { return L""; }
    bool setDefaultDevice(const std::wstring&, Flow, Role) override { return false; }
    std::wstring deviceName(const std::wstring&) override { return L""; }
    std::vector<SessionInfo> sessions(Flow) override { return {}; }
    bool setSessionVolume(const std::wstring&, float) override { return false; }
    bool setSessionMute(const std::wstring&, bool) override { return false; }
    bool setAppDevice(uint32_t, Flow, Role, const std::wstring&) override { return false; }
    std::wstring appDevice(uint32_t, Flow, Role) override { return L""; }
    bool clearAllAppDevices() override { return false; }
    bool start() override { return true; }
    void stop() override {}
};

NullAudioService& nullAudioService()
{
    static NullAudioService instance;
    return instance;
}

EventBus& globalEvents()
{
    static EventBus instance;
    return instance;
}

std::unique_ptr<audio::IAudioService>& globalAudio()
{
    static std::unique_ptr<audio::IAudioService> instance;
    return instance;
}

HWND g_mainWindow = nullptr;
bool g_storeDirty = false;

}  // namespace

audio::IAudioService& audioService()
{
    std::unique_ptr<audio::IAudioService>& audio = globalAudio();
    return audio ? *audio : nullAudioService();
}

EventBus& events()
{
    return globalEvents();
}

core::Store& store()
{
    return core::Store::instance();
}

HWND mainMessageWindow()
{
    return g_mainWindow;
}

void setAudioService(std::unique_ptr<audio::IAudioService> service)
{
    globalAudio() = std::move(service);
}

void setMainMessageWindow(HWND window)
{
    g_mainWindow = window;
}

void setStoreDirty(bool dirty)
{
    g_storeDirty = dirty;
}

bool storeDirty()
{
    return g_storeDirty;
}

}  // namespace soundint

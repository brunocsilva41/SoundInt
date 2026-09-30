// ============================================================================
// Instancia unica — implementacao (ver single_instance.h).
// ============================================================================
#include "app/single_instance.h"

#include "app/messages.h"

namespace soundint {

namespace {

constexpr wchar_t kMutexName[] = L"SoundInt.SingleInstance";
constexpr wchar_t kWindowClass[] = L"SoundIntMainWindow";

// A primeira instancia pode ainda estar no boot quando a segunda chega:
// espera a janela oculta aparecer (max ~1s) antes de desistir.
constexpr int kFindRetries = 20;
constexpr DWORD kFindRetryDelayMs = 50;

}  // namespace

SingleInstance::~SingleInstance()
{
    release();
}

bool SingleInstance::acquire()
{
    // Nao fechamos o handle enquanto o processo vive: o mutex so existe
    // enquanto houver um handle aberto.
    HANDLE handle = CreateMutexW(nullptr, TRUE, kMutexName);
    if (handle == nullptr) {
        // Sem mutex (falha rara de recursos): segue como instancia unica.
        primary_ = true;
        return true;
    }

    const DWORD status = GetLastError();
    if (status == ERROR_ALREADY_EXISTS) {
        CloseHandle(handle);
        mutex_ = nullptr;
        primary_ = false;
        return false;
    }

    mutex_ = handle;
    primary_ = true;
    return true;
}

bool SingleInstance::notifyExisting() const
{
    HWND window = nullptr;
    for (int attempt = 0; attempt < kFindRetries; ++attempt) {
        window = FindWindowW(kWindowClass, nullptr);
        if (window != nullptr) {
            break;
        }
        Sleep(kFindRetryDelayMs);
    }
    if (window == nullptr) {
        return false;
    }
    return PostMessageW(window, WM_APP_SHOW_REQUEST, kShowRequestMain, 0) != FALSE;
}

void SingleInstance::release()
{
    if (mutex_ != nullptr) {
        CloseHandle(mutex_);
        mutex_ = nullptr;
    }
    primary_ = false;
}

}  // namespace soundint

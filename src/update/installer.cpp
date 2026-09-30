// ============================================================================
// Spawn do instalador (Track F) — CreateProcessW em modo silencioso NSIS.
// ============================================================================
#include "update/installer.h"

#include <windows.h>

#include <vector>

namespace soundint::update::installer {
namespace {

// Texto legivel de um erro do sistema.
std::wstring systemErrorText(DWORD code)
{
    wchar_t buffer[512] = {};
    const DWORD capacity = static_cast<DWORD>(sizeof(buffer) / sizeof(buffer[0]));
    const DWORD flags = FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
    const DWORD length = FormatMessageW(flags, nullptr, code, 0, buffer, capacity, nullptr);

    std::wstring text;
    if (length > 0) {
        text.assign(buffer, length);
    } else {
        text = L"erro " + std::to_wstring(code);
    }
    while (!text.empty() && (text.back() == L'\r' || text.back() == L'\n')) {
        text.pop_back();
    }
    return text;
}

}  // namespace

bool spawn(const std::wstring& installerPath, std::wstring* error)
{
    auto fail = [error](const std::wstring& message) {
        if (error != nullptr) {
            *error = message;
        }
        return false;
    };

    if (installerPath.empty()) {
        return fail(L"caminho do instalador vazio");
    }
    if (GetFileAttributesW(installerPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        return fail(L"instalador nao encontrado: " + installerPath);
    }

    // lpCommandLine precisa ser buffer mutavel; argv[0] entre aspas.
    std::wstring commandLine = L"\"" + installerPath + L"\" /SILENT /SP- /NOCANCEL";
    std::vector<wchar_t> buffer(commandLine.begin(), commandLine.end());
    buffer.push_back(L'\0');

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};

    const BOOL created =
        CreateProcessW(installerPath.c_str(), buffer.data(), nullptr, nullptr, FALSE,
                       CREATE_DEFAULT_ERROR_MODE, nullptr, nullptr, &startup, &process);
    if (!created) {
        const DWORD code = GetLastError();
        return fail(L"falha ao iniciar o instalador: " + systemErrorText(code));
    }

    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    if (error != nullptr) {
        error->clear();
    }
    return true;
}

}  // namespace soundint::update::installer
